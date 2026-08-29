// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/contextual_cueing/contextual_cueing_enums.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_cueing {
namespace {

TEST(ContextualCueingMetricsTest, CreateEvent_EmptyCollections) {
  // Given
  tabs::MockTabInterface active_tab;
  EXPECT_CALL(active_tab, GetURL())
      .WillRepeatedly(testing::Return(GURL("https://active.com")));
  EXPECT_CALL(active_tab, GetTitle())
      .WillRepeatedly(testing::Return(u"Active Title"));

  // When
  auto event = internal::CreateContextualCueLogEvent(
      private_insights::events::ContextualCueLogEvent::SHOWN, "test_cue_id",
      CueTargetType::kGlic, {}, &active_tab,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"test_cuj");

  // Then
  EXPECT_EQ("test_cue_id", event.cue_id());
  EXPECT_EQ("https://active.com/", event.cue_context().active_page().url());
  EXPECT_EQ("Active Title", event.cue_context().active_page().title());
  EXPECT_EQ("[]", event.cue_context().recent_pages());
  EXPECT_EQ("[]", event.cue_context().tabs_shown());
  EXPECT_EQ("test_cuj", event.cue_details().cuj_type());
}

TEST(ContextualCueingMetricsTest, CreateEvent_NullTabToShow) {
  // Given
  tabs::MockTabInterface active_tab;
  EXPECT_CALL(active_tab, GetURL())
      .WillRepeatedly(testing::Return(GURL("https://active.com")));
  EXPECT_CALL(active_tab, GetTitle())
      .WillRepeatedly(testing::Return(u"Active Title"));

  tabs::TabHandle null_handle;  // Default constructor creates a null handle.
  std::vector<tabs::TabHandle> tabs_to_show = {null_handle};

  // When
  auto event = internal::CreateContextualCueLogEvent(
      private_insights::events::ContextualCueLogEvent::SHOWN, "test_cue_id",
      CueTargetType::kGlic, {}, &active_tab, tabs_to_show,
      /*background_tabs=*/{}, /*cuj=*/"test_cuj");

  // Then
  EXPECT_EQ("test_cue_id", event.cue_id());
  EXPECT_EQ("https://active.com/", event.cue_context().active_page().url());
  EXPECT_EQ("Active Title", event.cue_context().active_page().title());
  EXPECT_EQ("[]", event.cue_context().recent_pages());
  // The null handle should be skipped by the internal extractor.
  EXPECT_EQ("[]", event.cue_context().tabs_shown());
  EXPECT_EQ("test_cuj", event.cue_details().cuj_type());
}

TEST(ContextualCueingMetricsTest, CreateEvent_EmptyBackgroundTab) {
  // Given
  tabs::MockTabInterface active_tab;
  EXPECT_CALL(active_tab, GetURL())
      .WillRepeatedly(testing::Return(GURL("https://active.com")));
  EXPECT_CALL(active_tab, GetTitle())
      .WillRepeatedly(testing::Return(u"Active Title"));

  optimization_guide::proto::Tab empty_tab;
  std::vector<optimization_guide::proto::Tab> background_tabs = {empty_tab};

  // When
  auto event = internal::CreateContextualCueLogEvent(
      private_insights::events::ContextualCueLogEvent::SHOWN, "test_cue_id",
      CueTargetType::kGlic, {}, &active_tab,
      /*tabs_to_show=*/{}, background_tabs, /*cuj=*/"test_cuj");

  // Then
  EXPECT_EQ("test_cue_id", event.cue_id());
  EXPECT_EQ("https://active.com/", event.cue_context().active_page().url());
  EXPECT_EQ("Active Title", event.cue_context().active_page().title());
  EXPECT_EQ("[]", event.cue_context().tabs_shown());
  // An empty proto tab has empty strings for URL and Title.
  // The extractor returns them, and they are serialized.
  EXPECT_EQ("[{\"title\":\"\",\"url\":\"\"}]",
            event.cue_context().recent_pages());
  EXPECT_EQ("test_cuj", event.cue_details().cuj_type());
}

TEST(ContextualCueingMetricsTest, CreateEvent) {
  // Given
  tabs::MockTabInterface active_tab;
  EXPECT_CALL(active_tab, GetURL())
      .WillRepeatedly(testing::Return(GURL("https://active.com")));
  EXPECT_CALL(active_tab, GetTitle())
      .WillRepeatedly(testing::Return(u"Active Title"));

  tabs::MockTabInterface other_tab;
  EXPECT_CALL(other_tab, GetURL())
      .WillRepeatedly(testing::Return(GURL("https://other.com")));
  EXPECT_CALL(other_tab, GetTitle())
      .WillRepeatedly(testing::Return(u"Other Title"));

  tabs::TabHandle other_tab_handle = other_tab.GetHandle();
  std::vector<tabs::TabHandle> tabs_to_show = {other_tab_handle};

  optimization_guide::proto::Tab bg_tab;
  bg_tab.set_url("https://bg.com");
  bg_tab.set_title("Bg Title");
  std::vector<optimization_guide::proto::Tab> background_tabs = {bg_tab};

  // When
  auto event = internal::CreateContextualCueLogEvent(
      private_insights::events::ContextualCueLogEvent::SHOWN, "test_cue_id",
      CueTargetType::kGlic, {}, &active_tab, tabs_to_show, background_tabs,
      /*cuj=*/"custom_cuj");

  // Then
  EXPECT_EQ("test_cue_id", event.cue_id());
  EXPECT_EQ("https://active.com/", event.cue_context().active_page().url());
  EXPECT_EQ("Active Title", event.cue_context().active_page().title());

  // Verify recent_pages (background_tabs)
  EXPECT_EQ("[{\"title\":\"Bg Title\",\"url\":\"https://bg.com\"}]",
            event.cue_context().recent_pages());

  // Verify tabs_shown (tabs_to_show)
  EXPECT_EQ("[{\"title\":\"Other Title\",\"url\":\"https://other.com/\"}]",
            event.cue_context().tabs_shown());
  EXPECT_EQ("custom_cuj", event.cue_details().cuj_type());
}

TEST(ContextualCueingMetricsTest, RecordCueShownMetrics_Pdf) {
  base::HistogramTester histogram_tester;
  CueTabMetrics tab_metrics;
  RecordCueShownMetrics(ukm::kInvalidSourceId, "test_cuj", tab_metrics,
                        base::Milliseconds(100), /*is_pdf=*/true);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueShown",
                                      base::HashMetricName("test_cuj"), 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueShown.PageType.Pdf",
      base::HashMetricName("test_cuj"), 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueShownLatency",
                                      100, 1);
}

TEST(ContextualCueingMetricsTest, RecordCueShownMetrics_NonPdf) {
  base::HistogramTester histogram_tester;
  CueTabMetrics tab_metrics;
  RecordCueShownMetrics(ukm::kInvalidSourceId, "test_cuj", tab_metrics,
                        base::Milliseconds(100), /*is_pdf=*/false);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueShown",
                                      base::HashMetricName("test_cuj"), 1);
  histogram_tester.ExpectTotalCount("ContextualCueing.V2.CueShown.PageType.Pdf",
                                    0);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueShownLatency",
                                      100, 1);
}

TEST(ContextualCueingMetricsTest, RecordContextualCueingInteraction_Pdf) {
  base::HistogramTester histogram_tester;
  RecordContextualCueingInteraction(ContextualCueingInteraction::kCueClicked,
                                    "test_cuj", ukm::kInvalidSourceId,
                                    base::Seconds(5), /*is_pdf=*/true);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueInteraction",
                                      ContextualCueingInteraction::kCueClicked,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.PageType.Pdf",
      ContextualCueingInteraction::kCueClicked, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.Clicked",
      base::HashMetricName("test_cuj"), 1);
}

TEST(ContextualCueingMetricsTest, RecordContextualCueingInteraction_NonPdf) {
  base::HistogramTester histogram_tester;
  RecordContextualCueingInteraction(ContextualCueingInteraction::kCueClicked,
                                    "test_cuj", ukm::kInvalidSourceId,
                                    base::Seconds(5), /*is_pdf=*/false);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueInteraction",
                                      ContextualCueingInteraction::kCueClicked,
                                      1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.V2.CueInteraction.PageType.Pdf", 0);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.Clicked",
      base::HashMetricName("test_cuj"), 1);
}

}  // namespace
}  // namespace contextual_cueing
