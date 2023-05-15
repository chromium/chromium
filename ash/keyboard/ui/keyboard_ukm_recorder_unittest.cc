// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ukm_recorder.h"

#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_type.h"

namespace keyboard {

TEST(KeyboardUkmRecorderTest, RecordUkmWithEmptySource) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder test_recorder;
  test_recorder.UpdateRecording({ukm::MSBB});
  EXPECT_EQ(0u, test_recorder.entries_count());

  RecordUkmKeyboardShown(ukm::SourceId(), ui::TEXT_INPUT_TYPE_NONE);

  EXPECT_EQ(0u, test_recorder.sources_count());
  EXPECT_EQ(0u, test_recorder.entries_count());
}

TEST(KeyboardUkmRecorderTest, RecordUkmWithNavigationId) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder test_recorder;
  test_recorder.UpdateRecording({ukm::MSBB});
  ASSERT_EQ(0u, test_recorder.entries_count());

  const ukm::SourceId source =
      ukm::ConvertToSourceId(1, ukm::SourceIdType::NAVIGATION_ID);
  RecordUkmKeyboardShown(source, ui::TEXT_INPUT_TYPE_PASSWORD);

  EXPECT_EQ(0u, test_recorder.sources_count());
  EXPECT_EQ(1u, test_recorder.entries_count());
  const auto entries = test_recorder.GetEntriesByName("VirtualKeyboard.Open");
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entries[0], "TextInputType",
                                                 ui::TEXT_INPUT_TYPE_PASSWORD);
}

}  // namespace keyboard
