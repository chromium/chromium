// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/filename_util.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void SimulateNetworkQualityChangeOnNetworkThread(
    net::EffectiveConnectionType type) {
  network::NetworkService::GetNetworkServiceForTesting()
      ->network_quality_estimator()
      ->SimulateNetworkQualityChangeForTesting(type);
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets)
      total_count += bucket.count;
    if (total_count >= count)
      return;
    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

int GetHistogramSamplesSum(base::HistogramTester* histogram_tester,
                           const std::string& histogram_name) {
  const std::vector<base::Bucket> buckets =
      histogram_tester->GetAllSamples(histogram_name);
  size_t sum = 0;
  for (const auto& bucket : buckets)
    sum += (bucket.count * bucket.min);
  return sum;
}

class TestNetworkQualityObserver
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  explicit TestNetworkQualityObserver(network::NetworkQualityTracker* tracker)
      : run_loop_wait_effective_connection_type_(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
        run_loop_(std::make_unique<base::RunLoop>()),
        tracker_(tracker),
        effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    tracker_->AddEffectiveConnectionTypeObserver(this);
  }

  ~TestNetworkQualityObserver() override {
    tracker_->RemoveEffectiveConnectionTypeObserver(this);
  }

  // NetworkQualityTracker::EffectiveConnectionTypeObserver implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    net::EffectiveConnectionType queried_type =
        tracker_->GetEffectiveConnectionType();
    EXPECT_EQ(type, queried_type);

    effective_connection_type_ = type;
    if (effective_connection_type_ != run_loop_wait_effective_connection_type_)
      return;
    run_loop_->Quit();
  }

  void WaitForNotification(
      net::EffectiveConnectionType run_loop_wait_effective_connection_type) {
    if (effective_connection_type_ == run_loop_wait_effective_connection_type)
      return;
    ASSERT_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
              run_loop_wait_effective_connection_type);
    run_loop_wait_effective_connection_type_ =
        run_loop_wait_effective_connection_type;
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop());
  }

 private:
  net::EffectiveConnectionType run_loop_wait_effective_connection_type_;
  std::unique_ptr<base::RunLoop> run_loop_;
  network::NetworkQualityTracker* tracker_;
  net::EffectiveConnectionType effective_connection_type_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkQualityObserver);
};

}  // namespace

class NetworkQualityEstimatorPrefsBrowserTest : public InProcessBrowserTest {
 public:
  // Simulates a network quality change.
  void SimulateNetworkQualityChange(net::EffectiveConnectionType type) {
    if (!content::IsOutOfProcessNetworkService()) {
      content::GetNetworkTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&SimulateNetworkQualityChangeOnNetworkThread, type));
      return;
    }

    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    content::StoragePartition* partition =
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
    DCHECK(partition->GetNetworkContext());
    DCHECK(content::GetNetworkService());

    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterface(
        network_service_test.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    network_service_test->SimulateNetworkQualityChange(
        type, base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                             base::Unretained(&run_loop)));
    run_loop.Run();
  }

  base::HistogramTester histogram_tester;
};

// Verify that prefs are read at startup, and the read prefs are notified to the
// network quality estimator.
IN_PROC_BROWSER_TEST_F(NetworkQualityEstimatorPrefsBrowserTest,
                       ReadPrefsAtStartup) {
  // The check below ensures that "NQE.Prefs.ReadSize" contains at least one
  // sample. This implies that NQE was notified of the read prefs.
  RetryForHistogramUntilCountReached(&histogram_tester, "NQE.Prefs.ReadSize",
                                     1);
}

// Verify that prefs are read at startup.
IN_PROC_BROWSER_TEST_F(NetworkQualityEstimatorPrefsBrowserTest,
                       ReadPrefsAtStartupCustomPrefFile) {
  // The check below ensures that "NQE.Prefs.ReadSize" contains at least one
  // sample. This implies that NQE was notified of the read prefs.
  RetryForHistogramUntilCountReached(&histogram_tester, "NQE.Prefs.ReadSize",
                                     1);

  base::HistogramTester histogram_tester2;

  // Create network context with JSON pref store pointing to the temp file.
  mojo::PendingRemote<network::mojom::NetworkContext> network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->http_server_properties_path =
      browser()->profile()->GetPath().Append(
          FILE_PATH_LITERAL("Temp Network Persistent State"));

  auto state = base::MakeRefCounted<JsonPrefStore>(
      context_params->http_server_properties_path.value());

  base::DictionaryValue pref_value;
  base::Value value("2G");
  pref_value.Set("network_id_foo",
                 base::Value::ToUniquePtrValue(value.Clone()));
  state->SetValue("net.network_qualities",
                  base::Value::ToUniquePtrValue(pref_value.Clone()), 0);

  // Wait for the pending commit to finish before creating the network context.
  base::RunLoop loop;
  state->CommitPendingWrite(
      base::BindOnce([](base::RunLoop* loop) { loop->Quit(); }, &loop));
  loop.Run();

  content::GetNetworkService()->CreateNetworkContext(
      network_context.InitWithNewPipeAndPassReceiver(),
      std::move(context_params));

  RetryForHistogramUntilCountReached(&histogram_tester2, "NQE.Prefs.ReadSize",
                                     1);
  // Pref value must be read from the temp file.
  EXPECT_LE(1,
            GetHistogramSamplesSum(&histogram_tester2, "NQE.Prefs.ReadSize"));
}

// Verify that prefs are read at startup, and written to later.
IN_PROC_BROWSER_TEST_F(NetworkQualityEstimatorPrefsBrowserTest, PrefsWritten) {
  // The check below ensures that "NQE.Prefs.ReadSize" contains at least one
  // sample. This implies that NQE was notified of the read prefs.
  RetryForHistogramUntilCountReached(&histogram_tester, "NQE.Prefs.ReadSize",
                                     1);

  // Change in network quality is guaranteed to trigger a pref write.
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_2G);
  TestNetworkQualityObserver network_quality_observer(
      g_browser_process->network_quality_tracker());
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  RetryForHistogramUntilCountReached(&histogram_tester, "NQE.Prefs.WriteCount",
                                     1);
}
