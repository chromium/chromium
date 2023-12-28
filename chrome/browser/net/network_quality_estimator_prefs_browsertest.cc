// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/run_loop.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/filename_util.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
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

  TestNetworkQualityObserver(const TestNetworkQualityObserver&) = delete;
  TestNetworkQualityObserver& operator=(const TestNetworkQualityObserver&) =
      delete;

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
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  net::EffectiveConnectionType run_loop_wait_effective_connection_type_;
  std::unique_ptr<base::RunLoop> run_loop_;
  raw_ptr<network::NetworkQualityTracker> tracker_;
  net::EffectiveConnectionType effective_connection_type_;
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
        browser()->profile()->GetDefaultStoragePartition();
    DCHECK(partition->GetNetworkContext());
    DCHECK(content::GetNetworkService());

    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    network_service_test->SimulateNetworkQualityChange(
        type, base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                             base::Unretained(&run_loop)));
    run_loop.Run();
  }
};

// Verify that prefs are read at startup.
// Flaky on ChromeOS. See https://crbug.com/1484891
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ReadPrefsAtStartupCustomPrefFile \
  DISABLED_ReadPrefsAtStartupCustomPrefFile
#else
#define MAYBE_ReadPrefsAtStartupCustomPrefFile ReadPrefsAtStartupCustomPrefFile
#endif
IN_PROC_BROWSER_TEST_F(NetworkQualityEstimatorPrefsBrowserTest,
                       MAYBE_ReadPrefsAtStartupCustomPrefFile) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  // Create network context with JSON pref store pointing to the temp file.
  mojo::PendingRemote<network::mojom::NetworkContext> network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->cert_verifier_params = content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  context_params->file_paths = network::mojom::NetworkContextFilePaths::New();
  const base::FilePath data_path = browser()->profile()->GetPath().Append(
      FILE_PATH_LITERAL("Network For Testing"));
  context_params->file_paths->data_directory = data_path;
  context_params->file_paths->unsandboxed_data_path =
      browser()->profile()->GetPath();
  context_params->file_paths->http_server_properties_file_name =
      base::FilePath(FILE_PATH_LITERAL("Temp Network Persistent State"));
  context_params->file_paths->trigger_migration = true;

  base::CreateDirectory(data_path);
  auto state = base::MakeRefCounted<JsonPrefStore>(data_path.Append(
      *context_params->file_paths->http_server_properties_file_name));

  base::Value::Dict pref_value;
  pref_value.Set("network_id_foo", "2G");
  state->SetValue("net.network_qualities", base::Value(std::move(pref_value)),
                  0);

  // Wait for the pending commit to finish before creating the network context.
  base::RunLoop loop;
  state->CommitPendingWrite(
      base::BindOnce([](base::RunLoop* loop) { loop->Quit(); }, &loop));
  loop.Run();

  content::CreateNetworkContextInNetworkService(
      network_context.InitWithNewPipeAndPassReceiver(),
      std::move(context_params));
}

// Verify that prefs are read at startup, and written to later.
IN_PROC_BROWSER_TEST_F(NetworkQualityEstimatorPrefsBrowserTest, PrefsWritten) {
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_2G);
  TestNetworkQualityObserver network_quality_observer(
      g_browser_process->network_quality_tracker());
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
}
