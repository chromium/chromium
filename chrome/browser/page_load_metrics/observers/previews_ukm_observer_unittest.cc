// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_ukm_observer.h"

#include <memory>

#include "base/macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/previews/core/previews_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace content {
class NavigationHandle;
}

namespace previews {

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

class TestPreviewsUKMObserver : public PreviewsUKMObserver {
 public:
  TestPreviewsUKMObserver(bool lite_page_received,
                          bool noscript_on,
                          bool resource_loading_hints_on,
                          bool origin_opt_out_received,
                          bool save_data_enabled)
      : lite_page_received_(lite_page_received),
        noscript_on_(noscript_on),
        resource_loading_hints_on_(resource_loading_hints_on),
        origin_opt_out_received_(origin_opt_out_received),
        save_data_enabled_(save_data_enabled) {}

  ~TestPreviewsUKMObserver() override {}

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override {
    PreviewsUITabHelper* ui_tab_helper = PreviewsUITabHelper::FromWebContents(
        navigation_handle->GetWebContents());
    previews::PreviewsUserData* user_data =
        ui_tab_helper->CreatePreviewsUserDataForNavigationHandle(
            navigation_handle, 1u);

    if (noscript_on_) {
      content::PreviewsState previews_state =
          user_data->committed_previews_state();
      user_data->set_committed_previews_state(previews_state |=
                                              content::NOSCRIPT_ON);
    }

    if (resource_loading_hints_on_) {
      content::PreviewsState previews_state =
          user_data->committed_previews_state();
      user_data->set_committed_previews_state(
          previews_state |= content::RESOURCE_LOADING_HINTS_ON);
    }

    if (lite_page_received_) {
      content::PreviewsState previews_state =
          user_data->committed_previews_state();
      user_data->set_committed_previews_state(previews_state |=
                                              content::SERVER_LITE_PAGE_ON);
    }

    if (origin_opt_out_received_) {
      user_data->SetCacheControlNoTransformDirective();
    }

    return PreviewsUKMObserver::OnCommit(navigation_handle, source_id);
  }

 private:
  bool IsDataSaverEnabled(
      content::NavigationHandle* navigation_handle) const override {
    return save_data_enabled_;
  }

  bool lite_page_received_;
  bool noscript_on_;
  bool resource_loading_hints_on_;
  bool origin_opt_out_received_;
  const bool save_data_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestPreviewsUKMObserver);
};

class PreviewsUKMObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  PreviewsUKMObserverTest() {}
  ~PreviewsUKMObserverTest() override {}

  void RunTest(bool lite_page_received,
               bool noscript_on,
               bool resource_loading_hints_on,
               bool origin_opt_out,
               bool save_data_enabled) {
    lite_page_received_ = lite_page_received;
    noscript_on_ = noscript_on;
    resource_loading_hints_on_ = resource_loading_hints_on;
    origin_opt_out_ = origin_opt_out;
    save_data_enabled_ = save_data_enabled;
    NavigateAndCommit(GURL(kDefaultTestUrl));
  }

  void ValidateUKM(bool server_lofi_expected,
                   bool client_lofi_expected,
                   bool lite_page_expected,
                   bool noscript_expected,
                   bool resource_loading_hints_expected,
                   int opt_out_value,
                   bool origin_opt_out_expected,
                   bool save_data_enabled_expected) {
    using UkmEntry = ukm::builders::Previews;
    auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);
    if (!server_lofi_expected && !client_lofi_expected && !lite_page_expected &&
        !noscript_expected && !resource_loading_hints_expected &&
        opt_out_value == 0 && !origin_opt_out_expected &&
        !save_data_enabled_expected) {
      EXPECT_EQ(0u, entries.size());
      return;
    }
    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
      test_ukm_recorder().ExpectEntrySourceHasUrl(entry, GURL(kDefaultTestUrl));
      EXPECT_EQ(server_lofi_expected, test_ukm_recorder().EntryHasMetric(
                                          entry, UkmEntry::kserver_lofiName));
      EXPECT_EQ(client_lofi_expected, test_ukm_recorder().EntryHasMetric(
                                          entry, UkmEntry::kclient_lofiName));
      EXPECT_EQ(lite_page_expected, test_ukm_recorder().EntryHasMetric(
                                        entry, UkmEntry::klite_pageName));
      EXPECT_EQ(noscript_expected, test_ukm_recorder().EntryHasMetric(
                                       entry, UkmEntry::knoscriptName));
      EXPECT_EQ(resource_loading_hints_expected,
                test_ukm_recorder().EntryHasMetric(
                    entry, UkmEntry::kresource_loading_hintsName));
      EXPECT_EQ(opt_out_value != 0, test_ukm_recorder().EntryHasMetric(
                                        entry, UkmEntry::kopt_outName));
      if (opt_out_value != 0) {
        test_ukm_recorder().ExpectEntryMetric(entry, UkmEntry::kopt_outName,
                                              opt_out_value);
      }
      EXPECT_EQ(origin_opt_out_expected,
                test_ukm_recorder().EntryHasMetric(
                    entry, UkmEntry::korigin_opt_outName));
      EXPECT_EQ(save_data_enabled_expected,
                test_ukm_recorder().EntryHasMetric(
                    entry, UkmEntry::ksave_data_enabledName));
    }
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness ::SetUp();
    PreviewsUITabHelper::CreateForWebContents(web_contents());
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<TestPreviewsUKMObserver>(
        lite_page_received_, noscript_on_, resource_loading_hints_on_,
        origin_opt_out_, save_data_enabled_));
    // Data is only added to the first navigation after RunTest().
    lite_page_received_ = false;
    noscript_on_ = false;
    resource_loading_hints_on_ = false;
    origin_opt_out_ = false;
  }

 private:
  bool lite_page_received_ = false;
  bool noscript_on_ = false;
  bool resource_loading_hints_on_ = false;
  bool origin_opt_out_ = false;
  bool save_data_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(PreviewsUKMObserverTest);
};

TEST_F(PreviewsUKMObserverTest, NoPreviewSeen) {
  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, UntrackedPreviewTypeOptOut) {
  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);
  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  // Opt out should not be added since we don't track this type.
  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, LitePageSeen) {
  RunTest(true /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, true /* lite_page_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, LitePageOptOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {} /* enabled features */,
      {previews::features::
           kAndroidOmniboxPreviewsBadge} /* disabled features */);

  RunTest(true /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, true /* lite_page_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              1 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, LitePageOptOutChip) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kAndroidOmniboxPreviewsBadge} /* enabled features */,
      {} /*disabled features */);

  RunTest(true /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, true /* lite_page_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              2 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, NoScriptSeen) {
  RunTest(false /* lite_page_received */, true /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              true /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, NoScriptOptOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {} /* enabled features */,
      {previews::features::
           kAndroidOmniboxPreviewsBadge} /* disabled features */);

  RunTest(false /* lite_page_received */, true /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              true /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              1 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, NoScriptOptOutChip) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kAndroidOmniboxPreviewsBadge} /* enabled features */,
      {} /*disabled features */);

  RunTest(false /* lite_page_received */, true /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              true /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              2 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, ResourceLoadingHintsSeen) {
  RunTest(false /* lite_page_received */, false /* noscript_on */,
          true /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */,
              true /* resource_loading_hints_expected */, 0 /* opt_out_value */,
              false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, ResourceLoadingHintsOptOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {} /* enabled features */,
      {previews::features::
           kAndroidOmniboxPreviewsBadge} /* disabled features */);

  RunTest(false /* lite_page_received */, false /* noscript_on */,
          true /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */,
              true /* resource_loading_hints_expected */, 1 /* opt_out_value */,
              false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, ResourceLoadingHintsOptOutChip) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kAndroidOmniboxPreviewsBadge} /* enabled features */,
      {} /*disabled features */);

  RunTest(false /* lite_page_received */, false /* noscript_on */,
          true /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */,
              true /* resource_loading_hints_expected */, 2 /* opt_out_value */,
              false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, ClientLoFiSeen) {
  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_client_lofi_requested(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      // Uncached non-proxied request.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, ClientLoFiOptOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {} /* enabled features */,
      {previews::features::
           kAndroidOmniboxPreviewsBadge} /* disabled features */);

  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_client_lofi_requested(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      // Uncached non-proxied request.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);
  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              1 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, ClientLoFiOptOutChip) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kAndroidOmniboxPreviewsBadge} /* enabled features */,
      {} /*disabled features */);

  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_client_lofi_requested(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      // Uncached non-proxied request.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);
  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              2 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, ServerLoFiSeen) {
  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_used_data_reduction_proxy(true);
  data->set_lofi_received(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  NavigateToUntrackedUrl();

  ValidateUKM(true /* server_lofi_expected */, false /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, ServerLoFiOptOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {} /* enabled features */,
      {previews::features::
           kAndroidOmniboxPreviewsBadge} /* disabled features */);

  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_used_data_reduction_proxy(true);
  data->set_lofi_received(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(true /* server_lofi_expected */, false /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              1 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, ServerLoFiOptOutChip) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kAndroidOmniboxPreviewsBadge} /* enabled features */,
      {} /*disabled features */);

  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_used_data_reduction_proxy(true);
  data->set_lofi_received(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(true /* server_lofi_expected */, false /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              2 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, BothLoFiSeen) {
  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data1 =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data1->set_used_data_reduction_proxy(true);
  data1->set_lofi_received(true);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data2 =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data2->set_used_data_reduction_proxy(true);
  data2->set_client_lofi_requested(true);

  // Prepare 4 resources of varying size and configurations, 1 has Client LoFi,
  // 1 has Server LoFi.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Uncached proxied request with .1 compression ratio.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 10 /* original_network_content_length */, std::move(data1),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      // Uncached proxied request with .5 compression ratio.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data2),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  NavigateToUntrackedUrl();
  ValidateUKM(true /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, BothLoFiOptOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {} /* enabled features */,
      {previews::features::
           kAndroidOmniboxPreviewsBadge} /* disabled features */);

  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data1 =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data1->set_used_data_reduction_proxy(true);
  data1->set_lofi_received(true);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data2 =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data2->set_used_data_reduction_proxy(true);
  data2->set_client_lofi_requested(true);

  // Prepare 4 resources of varying size and configurations, 1 has Client LoFi,
  // 1 has Server LoFi.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Uncached proxied request with .1 compression ratio.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 10 /* original_network_content_length */, std::move(data1),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      // Uncached proxied request with .5 compression ratio.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data2),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);
  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();
  ValidateUKM(true /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              1 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, BothLoFiOptOutChip) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kAndroidOmniboxPreviewsBadge} /* enabled features */,
      {} /*disabled features */);

  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data1 =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data1->set_used_data_reduction_proxy(true);
  data1->set_lofi_received(true);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data2 =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data2->set_used_data_reduction_proxy(true);
  data2->set_client_lofi_requested(true);

  // Prepare 4 resources of varying size and configurations, 1 has Client LoFi,
  // 1 has Server LoFi.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Uncached proxied request with .1 compression ratio.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 10 /* original_network_content_length */, std::move(data1),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
      // Uncached proxied request with .5 compression ratio.
      {GURL(kDefaultTestUrl), net::HostPortPair(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data2),
       content::ResourceType::RESOURCE_TYPE_IMAGE, 0,
       nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);
  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();
  ValidateUKM(true /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */, false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              2 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, OriginOptOut) {
  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, true /* origin_opt_out */,
          false /* save_data_enabled */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, true /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, DataSaverEnabled) {
  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, CheckReportingForHidden) {
  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */);

  web_contents()->WasHidden();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */);
}

TEST_F(PreviewsUKMObserverTest, CheckReportingForFlushMetrics) {
  RunTest(false /* lite_page_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */);

  SimulateAppEnterBackground();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */);
}

}  // namespace

}  // namespace previews
