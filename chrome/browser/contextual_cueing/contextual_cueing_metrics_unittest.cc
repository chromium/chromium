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
  EXPECT_EQ("active.com", event.cue_context().active_page().hostname());
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

// Verifies that `active_page.hostname` holds the host component of the active
// tab's URL, as returned by `GURL::host()`.
struct HostTestCase {
  const char* test_name;
  const char* url;
  const char* expected_host;
};

const HostTestCase kHostTestCases[] = {
    {"BareDomain", "https://active.com/", "active.com"},
    // Subdomains are preserved; this is what distinguishes the host from an
    // eTLD+1.
    {"Subdomain", "https://www.active.com/", "www.active.com"},
    {"NestedSubdomains", "https://www.foo.bar.active.com/",
     "www.foo.bar.active.com"},
    {"MultiLevelPublicSuffix", "https://a.b.co.uk/", "a.b.co.uk"},
    {"PrivateRegistry", "https://myblog.blogspot.com/", "myblog.blogspot.com"},
    // Port, path, query, and fragment are not part of the host.
    {"PortPathQueryFragment", "https://www.active.com:8080/a/b?q=1#frag",
     "www.active.com"},
    // Userinfo is not part of the host.
    {"Userinfo", "https://user:pass@www.active.com/", "www.active.com"},
    // GURL canonicalizes the host to lowercase.
    {"Uppercase", "https://WWW.Active.COM/", "www.active.com"},
    // Hosts that have no registrable domain are still returned verbatim.
    {"IpAddress", "http://192.168.0.1/", "192.168.0.1"},
    {"SingleComponentHost", "http://localhost:3000/", "localhost"},
    // URLs without a host yield an empty string.
    {"FileUrl", "file:///tmp/bar.html", ""},
    {"AboutBlank", "about:blank", ""},
};

class ContextualCueingMetricsHostTest
    : public testing::TestWithParam<HostTestCase> {};

TEST_P(ContextualCueingMetricsHostTest, CreateEventPopulatesHost) {
  // Given
  const HostTestCase& test_case = GetParam();
  tabs::MockTabInterface active_tab;
  EXPECT_CALL(active_tab, GetURL())
      .WillRepeatedly(testing::Return(GURL(test_case.url)));
  EXPECT_CALL(active_tab, GetTitle())
      .WillRepeatedly(testing::Return(u"Active Title"));

  // When
  auto event = internal::CreateContextualCueLogEvent(
      private_insights::events::ContextualCueLogEvent::SHOWN, "test_cue_id",
      CueTargetType::kGlic, {}, &active_tab,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"test_cuj");

  // Then
  EXPECT_EQ(test_case.expected_host,
            event.cue_context().active_page().hostname());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualCueingMetricsHostTest,
    testing::ValuesIn(kHostTestCases),
    [](const testing::TestParamInfo<HostTestCase>& info) {
      return info.param.test_name;
    });

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
