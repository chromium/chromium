// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_base.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/optional.h"
#include "base/process/kill.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_test_utils.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/previews/content/previews_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/ip_endpoint.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace data_reduction_proxy {

namespace {

// DataReductionProxyMetricsObserver responsible for modifying data about the
// navigation in OnCommit.
class TestDataReductionProxyMetricsObserverBase
    : public DataReductionProxyMetricsObserverBase {
 public:
  TestDataReductionProxyMetricsObserverBase(
      content::WebContents* web_contents,
      bool data_reduction_proxy_used,
      bool cached_data_reduction_proxy_used,
      bool lite_page_used,
      bool black_listed,
      std::string session_key,
      uint64_t page_id)
      : web_contents_(web_contents),
        data_reduction_proxy_used_(data_reduction_proxy_used),
        cached_data_reduction_proxy_used_(cached_data_reduction_proxy_used),
        lite_page_used_(lite_page_used),
        black_listed_(black_listed),
        session_key_(session_key),
        page_id_(page_id) {}

  ~TestDataReductionProxyMetricsObserverBase() override {}

  // DataReductionProxyMetricsObserverBase:
  ObservePolicy OnCommitCalled(content::NavigationHandle* navigation_handle,
                               ukm::SourceId source_id) override {
    auto data =
        std::make_unique<data_reduction_proxy::DataReductionProxyData>();
    data->set_request_url(navigation_handle->GetURL());
    data->set_used_data_reduction_proxy(data_reduction_proxy_used_);
    data->set_was_cached_data_reduction_proxy_response(
        cached_data_reduction_proxy_used_);
    data->set_request_url(GURL(kDefaultTestUrl));
    data->set_lite_page_received(lite_page_used_);
    data->set_session_key(session_key_);
    data->set_page_id(page_id_);
    DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
        web_contents_->GetBrowserContext())
        ->SetDataForNextCommitForTesting(std::move(data));

    auto* previews_data = PreviewsDataForNavigationHandle(navigation_handle);
    previews_data->set_black_listed_for_lite_page(black_listed_);

    return DataReductionProxyMetricsObserverBase::OnCommitCalled(
        navigation_handle, source_id);
  }

 private:
  content::WebContents* web_contents_;
  bool data_reduction_proxy_used_;
  bool cached_data_reduction_proxy_used_;
  bool lite_page_used_;
  bool black_listed_;
  std::string session_key_;
  uint64_t page_id_;

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyMetricsObserverBase);
};

class DataReductionProxyMetricsObserverBaseTest
    : public DataReductionProxyMetricsObserverTestBase {
 public:
  void ValidateUKM(bool want_ukm,
                   uint64_t want_original_bytes,
                   uint64_t want_uuid) {
    using UkmEntry = ukm::builders::DataReductionProxy;
    auto entries =
        tester()->test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);

    if (!want_ukm) {
      EXPECT_EQ(0u, entries.size());
      return;
    }

    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
      tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(
          entry, GURL(kDefaultTestUrl));

      tester()->test_ukm_recorder().ExpectEntryMetric(
          entry, UkmEntry::kEstimatedOriginalNetworkBytesName,
          want_original_bytes);
      tester()->test_ukm_recorder().ExpectEntryMetric(
          entry, UkmEntry::kDataSaverPageUUIDName, want_uuid);
    }
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestDataReductionProxyMetricsObserverBase>(
            web_contents(), data_reduction_proxy_used(),
            cached_data_reduction_proxy_used(), is_using_lite_page(),
            black_listed(), session_key(), page_id()));
  }
};

}  // namespace

TEST_F(DataReductionProxyMetricsObserverBaseTest,
       ValidateUKM_DataSaverNotUsed) {
  ResetTest();
  RunTestAndNavigateToUntrackedUrl(false, false, false);
  ValidateUKM(false, 0U, 0U);
}

TEST_F(DataReductionProxyMetricsObserverBaseTest, ValidateUKM_DataSaverUsed) {
  ResetTest();
  RunTestAndNavigateToUntrackedUrl(true, false, false);
  ValidateUKM(true, 0U, 0U);
}

TEST_F(DataReductionProxyMetricsObserverBaseTest,
       ValidateUKM_DataSaverPopulated_Lower) {
  ResetTest();

  session_key_ = "abc";
  page_id_ = 1U;

  RunTest(true, false, false, false);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateDataReductionProxyResource(
      false /* was_cached */, 2 * 1024 /* delta_bytes */,
      true /* is_complete */, true /* proxy_used*/, 5 /* compression_ratio */));

  tester()->SimulateResourceDataUseUpdate(resources);

  tester()->NavigateToUntrackedUrl();
  // Use https://play.golang.org/p/XnMDcTiPzt8 to generate an updated uuid.
  ValidateUKM(true, 9918U, 5261012271403106530);
}

TEST_F(DataReductionProxyMetricsObserverBaseTest,
       ValidateUKM_DataSaverPopulated_Upper) {
  ResetTest();

  session_key_ = "xyz";
  // Same as 1<<63, but keeps clang happy.
  page_id_ = 0x8000000000000000;

  RunTest(true, false, false, false);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateDataReductionProxyResource(
      false /* was_cached */, 2 * 1024 /* delta_bytes */,
      true /* is_complete */, true /* proxy_used*/, 5 /* compression_ratio */));

  tester()->SimulateResourceDataUseUpdate(resources);

  tester()->NavigateToUntrackedUrl();
  // Use https://play.golang.org/p/XnMDcTiPzt8 to generate an updated uuid.
  ValidateUKM(true, 9918U, 7538012597823726222);
}

}  // namespace data_reduction_proxy
