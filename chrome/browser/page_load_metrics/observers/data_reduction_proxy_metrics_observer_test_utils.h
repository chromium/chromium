// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_TEST_UTILS_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_TEST_UTILS_H_

#include <stdint.h>
#include <functional>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/previews/content/previews_user_data.h"
#include "net/nqe/effective_connection_type.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace data_reduction_proxy {

const char kDefaultTestUrl[] = "http://google.com";

// Attaches a new |PreviewsUserData| to the given |navigation_handle|.
previews::PreviewsUserData* PreviewsDataForNavigationHandle(
    content::NavigationHandle* navigation_handle);

page_load_metrics::mojom::ResourceDataUpdatePtr
CreateDataReductionProxyResource(bool was_cached,
                                 int64_t delta_bytes,
                                 bool is_complete,
                                 bool proxy_used,
                                 double compression_ratio = 1.0);

// This base test class does all the test support and validation associated with
// the DRP metrics.
class DataReductionProxyMetricsObserverTestBase
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  DataReductionProxyMetricsObserverTestBase();
  ~DataReductionProxyMetricsObserverTestBase() override;

  // Resets all testing state. Should be called before every test case.
  void ResetTest();

  // Navigates and commits to |kDefaultTestUrl| and mocks a single timing
  // update with the given data reduction proxy state.
  void RunTest(bool data_reduction_proxy_used,
               bool is_using_lite_page,
               bool opt_out_expected,
               bool black_listed);

  // Navigates and commits to |kDefaultTestUrl| and mocks a single timing
  // update with the given lite page redirect previews state.
  void RunLitePageRedirectTest(
      previews::PreviewsUserData::ServerLitePageInfo* preview_info,
      net::EffectiveConnectionType ect);

  // The same as |RunTest| but also navigates to an untracked URL afterwards.
  void RunTestAndNavigateToUntrackedUrl(bool data_reduction_proxy_used,
                                        bool is_using_lite_page,
                                        bool opt_out_expected);

  // Verify that, if expected and actual are set, their values are equal.
  // Otherwise, verify that both are unset.
  void ExpectEqualOrUnset(const base::Optional<base::TimeDelta>& expected,
                          const base::Optional<base::TimeDelta>& actual);

  // Set ups test state.
  void SetUp() override;

  page_load_metrics::mojom::PageLoadTimingPtr timing() {
    return timing_.Clone();
  }
  bool cached_data_reduction_proxy_used() const {
    return cached_data_reduction_proxy_used_;
  }
  previews::PreviewsUserData::ServerLitePageInfo* preview_info() const {
    return preview_info_;
  }
  net::EffectiveConnectionType ect() const { return ect_; }
  bool data_reduction_proxy_used() const { return data_reduction_proxy_used_; }
  bool is_using_lite_page() const { return is_using_lite_page_; }
  bool opt_out_expected() const { return opt_out_expected_; }
  bool black_listed() const { return black_listed_; }
  std::string session_key() const { return session_key_; }
  uint64_t page_id() const { return page_id_; }

 protected:
  page_load_metrics::mojom::PageLoadTiming timing_;
  bool cached_data_reduction_proxy_used_ = false;
  std::string session_key_;
  uint64_t page_id_ = 0;

 private:
  previews::PreviewsUserData::ServerLitePageInfo* preview_info_;
  net::EffectiveConnectionType ect_;
  bool data_reduction_proxy_used_;
  bool is_using_lite_page_;
  bool opt_out_expected_;
  bool black_listed_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMetricsObserverTestBase);
};

}  // namespace data_reduction_proxy

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_TEST_UTILS_H_
