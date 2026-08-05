// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
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
      /*tabs_to_show=*/{}, /*background_tabs=*/{});

  // Then
  EXPECT_EQ("test_cue_id", event.cue_id());
  EXPECT_EQ("https://active.com/", event.cue_context().active_page().url());
  EXPECT_EQ("Active Title", event.cue_context().active_page().title());
  EXPECT_EQ("[]", event.cue_context().recent_pages());
  EXPECT_EQ("[]", event.cue_context().tabs_shown());
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
      /*background_tabs=*/{});

  // Then
  EXPECT_EQ("test_cue_id", event.cue_id());
  EXPECT_EQ("https://active.com/", event.cue_context().active_page().url());
  EXPECT_EQ("Active Title", event.cue_context().active_page().title());
  EXPECT_EQ("[]", event.cue_context().recent_pages());
  // The null handle should be skipped by the internal extractor.
  EXPECT_EQ("[]", event.cue_context().tabs_shown());
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
      /*tabs_to_show=*/{}, background_tabs);

  // Then
  EXPECT_EQ("test_cue_id", event.cue_id());
  EXPECT_EQ("https://active.com/", event.cue_context().active_page().url());
  EXPECT_EQ("Active Title", event.cue_context().active_page().title());
  EXPECT_EQ("[]", event.cue_context().tabs_shown());
  // An empty proto tab has empty strings for URL and Title.
  // The extractor returns them, and they are serialized.
  EXPECT_EQ("[{\"title\":\"\",\"url\":\"\"}]",
            event.cue_context().recent_pages());
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
      CueTargetType::kGlic, {}, &active_tab, tabs_to_show, background_tabs);

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
}

}  // namespace
}  // namespace contextual_cueing
