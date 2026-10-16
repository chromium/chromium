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
#include "components/metrics/version_utils.h"
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
  EXPECT_EQ("active.com", event.cue_context().active_page().hostname());
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
  EXPECT_EQ("active.com", event.cue_context().active_page().hostname());
  EXPECT_EQ("[]", event.cue_context().recent_pages());
  // The null handle should be skipped by the internal extractor.
  EXPECT_EQ("[]", event.cue_context().tabs_shown());
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
  EXPECT_EQ("active.com", event.cue_context().active_page().hostname());
  EXPECT_EQ("Active Title", event.cue_context().active_page().title());

  // Verify recent_pages (background_tabs)
  EXPECT_EQ("[{\"title\":\"Bg Title\",\"url\":\"https://bg.com\"}]",
            event.cue_context().recent_pages());

  // Verify tabs_shown (tabs_to_show)
  EXPECT_EQ("[{\"title\":\"Other Title\",\"url\":\"https://other.com/\"}]",
            event.cue_context().tabs_shown());
  EXPECT_EQ("custom_cuj", event.cue_details().cuj_type());
  EXPECT_EQ(metrics::GetOperatingSystemName(),
            event.system_profile().platform());
  EXPECT_FALSE(event.system_profile().platform().empty());
}

TEST(ContextualCueingMetricsTest, CreateEvent_NoActiveTab) {
  // When
  auto event = internal::CreateContextualCueLogEvent(
      private_insights::events::ContextualCueLogEvent::SHOWN, "test_cue_id",
      CueTargetType::kGlic, {}, /*active_tab=*/nullptr,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"test_cuj");

  // Then
  EXPECT_EQ("test_cue_id", event.cue_id());
  // With no active tab, the active page should be left entirely unset rather
  // than populated with empty strings.
  EXPECT_FALSE(event.cue_context().has_active_page());
  EXPECT_EQ("[]", event.cue_context().recent_pages());
  EXPECT_EQ("[]", event.cue_context().tabs_shown());
  EXPECT_EQ("test_cuj", event.cue_details().cuj_type());
}

TEST(ContextualCueingMetricsTest, CreateEvent_ContextualSearchSurface) {
  optimization_guide::proto::ContextualCue cue;
  cue.mutable_anchored_message_cue()->set_anchored_message_text("Message");
  cue.mutable_anchored_message_cue()->set_action_text("Action");
  cue.mutable_contextual_search_surface()->set_query("search query");

  auto event = internal::CreateContextualCueLogEvent(
      private_insights::events::ContextualCueLogEvent::SHOWN, "test_cue_id",
      CueTargetType::kContextualSearch, cue, /*active_tab=*/nullptr,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"LEARNING");

  EXPECT_EQ("ContextualSearch", event.cue_details().promoted_feature());
  EXPECT_EQ("search query", event.cue_details().prompt());
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

namespace contextual_cueing {

TEST(ContextualCueingMetricsTest, RecordCueFormFactorShown_EmptyCuj) {
  base::HistogramTester histogram_tester;
  RecordCueFormFactorShown("", CueFormFactor::kChip);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueFormFactor.Shown",
                                      CueFormFactor::kChip, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.V2.CueFormFactor.Shown.Chip", 0);
}

TEST(ContextualCueingMetricsTest, RecordCueFormFactorShown_WithCuj) {
  base::HistogramTester histogram_tester;
  RecordCueFormFactorShown("test_cuj", CueFormFactor::kIcon);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueFormFactor.Shown",
                                      CueFormFactor::kIcon, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueFormFactor.Shown.Icon",
      base::HashMetricName("test_cuj"), 1);
}

TEST(ContextualCueingMetricsTest, RecordCueFormFactorHidden_EmptyCuj) {
  base::HistogramTester histogram_tester;
  RecordCueFormFactorHidden("", CueFormFactor::kChip);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueFormFactor.Hidden", CueFormFactor::kChip, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.V2.CueFormFactor.Hidden.Chip", 0);
}

TEST(ContextualCueingMetricsTest, RecordCueFormFactorHidden_WithCuj) {
  base::HistogramTester histogram_tester;
  RecordCueFormFactorHidden("test_cuj", CueFormFactor::kAnchoredMessage);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueFormFactor.Hidden",
      CueFormFactor::kAnchoredMessage, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueFormFactor.Hidden.AnchoredMessage",
      base::HashMetricName("test_cuj"), 1);
}

}  // namespace contextual_cueing
