// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_hub {
namespace {

using ::browser::context_hub::mojom::SkillRecordingActionType;

TEST(RecordingArtifactsTest, StoresRecordedActionsAndScreenshots) {
  RecordingArtifacts artifacts;
  EXPECT_TRUE(artifacts.actions.empty());

  artifacts.actions.emplace_back(SkillRecordingActionType::kLeftClick,
                                 "Mouse Clicked at X:10, Y:20",
                                 base::Milliseconds(150));
  artifacts.actions[0].screenshot_jpeg_bytes =
      std::string("\x01\x02\x03\x04", 4);
  artifacts.actions.emplace_back(SkillRecordingActionType::kLeftClick,
                                 "Mouse Clicked at X:30, Y:40",
                                 base::Milliseconds(350));

  ASSERT_EQ(artifacts.actions.size(), 2u);
  EXPECT_EQ(artifacts.actions[0].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(artifacts.actions[0].description, "Mouse Clicked at X:10, Y:20");
  EXPECT_EQ(artifacts.actions[0].timestamp, base::Milliseconds(150));
  EXPECT_EQ(artifacts.actions[0].screenshot_jpeg_bytes,
            std::string("\x01\x02\x03\x04", 4));

  EXPECT_EQ(artifacts.actions[1].type, SkillRecordingActionType::kLeftClick);
  EXPECT_EQ(artifacts.actions[1].description, "Mouse Clicked at X:30, Y:40");
  EXPECT_EQ(artifacts.actions[1].timestamp, base::Milliseconds(350));
  EXPECT_TRUE(artifacts.actions[1].screenshot_jpeg_bytes.empty());
}

}  // namespace
}  // namespace context_hub
