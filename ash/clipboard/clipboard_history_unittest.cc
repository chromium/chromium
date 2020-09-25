// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history.h"

#include <list>
#include <unordered_map>

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class ClipboardHistoryTest : public AshTestBase {
 public:
  ClipboardHistoryTest() = default;
  ClipboardHistoryTest(const ClipboardHistoryTest&) = delete;
  ClipboardHistoryTest& operator=(const ClipboardHistoryTest&) = delete;
  ~ClipboardHistoryTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kClipboardHistory}, {});
    AshTestBase::SetUp();
    clipboard_history_ = const_cast<ClipboardHistory*>(
        Shell::Get()->clipboard_history_controller()->history());
  }

  const std::list<ClipboardHistoryItem>& GetClipboardHistoryItems() {
    return clipboard_history_->GetItems();
  }

  // Writes |input_strings| to the clipboard buffer and ensures that
  // |expected_strings| are retained in history. If |in_same_sequence| is true,
  // writes to the buffer will be performed in the same task sequence.
  void WriteAndEnsureTextHistory(
      const std::vector<base::string16>& input_strings,
      const std::vector<base::string16>& expected_strings,
      bool in_same_sequence = false) {
    for (const auto& input_string : input_strings) {
      {
        ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
        scw.WriteText(input_string);
      }
      if (!in_same_sequence)
        base::RunLoop().RunUntilIdle();
    }
    if (in_same_sequence)
      base::RunLoop().RunUntilIdle();
    EnsureTextHistory(expected_strings);
  }

  void EnsureTextHistory(const std::vector<base::string16>& expected_strings) {
    const std::list<ClipboardHistoryItem>& items = GetClipboardHistoryItems();
    EXPECT_EQ(expected_strings.size(), items.size());

    int expected_strings_index = 0;
    for (const auto& item : items) {
      EXPECT_EQ(expected_strings[expected_strings_index++],
                base::UTF8ToUTF16(item.data().text()));
    }
  }

  // Writes |input_bitmaps| to the clipboard buffer and ensures that
  // |expected_bitmaps| are retained in history. Writes to the buffer are
  // performed in different task sequences to simulate real world behavior.
  void WriteAndEnsureBitmapHistory(std::vector<SkBitmap>& input_bitmaps,
                                   std::vector<SkBitmap>& expected_bitmaps) {
    for (const auto& input_bitmap : input_bitmaps) {
      {
        ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
        scw.WriteImage(input_bitmap);
      }
      base::RunLoop().RunUntilIdle();
    }
    const std::list<ClipboardHistoryItem>& items = GetClipboardHistoryItems();
    EXPECT_EQ(expected_bitmaps.size(), items.size());

    int expected_bitmaps_index = 0;
    for (const auto& item : items) {
      EXPECT_TRUE(gfx::BitmapsAreEqual(
          expected_bitmaps[expected_bitmaps_index++], item.data().bitmap()));
    }
  }

  // Writes |input_data| to the clipboard buffer and ensures that
  // |expected_data| is retained in history. After writing to the buffer, the
  // current task sequence is run to idle to simulate real world behavior.
  void WriteAndEnsureCustomDataHistory(
      const std::unordered_map<base::string16, base::string16>& input_data,
      const std::unordered_map<base::string16, base::string16>& expected_data) {
    base::Pickle input_data_pickle;
    ui::WriteCustomDataToPickle(input_data, &input_data_pickle);

    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WritePickledData(input_data_pickle,
                           ui::ClipboardFormatType::GetWebCustomDataType());
    }
    base::RunLoop().RunUntilIdle();

    const std::list<ClipboardHistoryItem> items = GetClipboardHistoryItems();
    EXPECT_EQ(expected_data.empty() ? 0u : 1u, items.size());

    if (expected_data.empty())
      return;

    std::unordered_map<base::string16, base::string16> actual_data;
    ui::ReadCustomDataIntoMap(items.front().data().custom_data_data().c_str(),
                              items.front().data().custom_data_data().size(),
                              &actual_data);

    EXPECT_EQ(expected_data, actual_data);
  }

  ClipboardHistory* clipboard_history() { return clipboard_history_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  // Owned by ClipboardHistoryControllerImpl.
  ClipboardHistory* clipboard_history_ = nullptr;
};

// Tests that with nothing copied, nothing is shown.
TEST_F(ClipboardHistoryTest, NothingCopiedNothingSaved) {
  // When nothing is copied, nothing should be saved.
  WriteAndEnsureTextHistory(/*input_strings=*/{},
                            /*expected_strings=*/{});
}

// Tests that if one thing is copied, one thing is saved.
TEST_F(ClipboardHistoryTest, OneThingCopiedOneThingSaved) {
  std::vector<base::string16> input_strings{base::UTF8ToUTF16("test")};
  std::vector<base::string16> expected_strings = input_strings;

  // Test that only one string is in history.
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that if the same (non bitmap) thing is copied, both things are saved.
TEST_F(ClipboardHistoryTest, DuplicateBasic) {
  std::vector<base::string16> input_strings{base::UTF8ToUTF16("test"),
                                            base::UTF8ToUTF16("test")};
  std::vector<base::string16> expected_strings = input_strings;

  // Test that both things are saved.
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that if multiple things are copied in the same task sequence, only the
// most recent thing is saved.
TEST_F(ClipboardHistoryTest, InSameSequenceBasic) {
  std::vector<base::string16> input_strings{base::UTF8ToUTF16("test1"),
                                            base::UTF8ToUTF16("test2"),
                                            base::UTF8ToUTF16("test3")};
  // Because |input_strings| will be copied in the same task sequence, history
  // should only retain the most recent thing.
  std::vector<base::string16> expected_strings{base::UTF8ToUTF16("test3")};

  // Test that only the most recent thing is saved.
  WriteAndEnsureTextHistory(input_strings, expected_strings,
                            /*in_same_sequence=*/true);
}

// Tests the ordering of history is in reverse chronological order.
TEST_F(ClipboardHistoryTest, HistoryIsReverseChronological) {
  std::vector<base::string16> input_strings{
      base::UTF8ToUTF16("test1"), base::UTF8ToUTF16("test2"),
      base::UTF8ToUTF16("test3"), base::UTF8ToUTF16("test4")};
  std::vector<base::string16> expected_strings = input_strings;
  // Reverse the vector, history should match this ordering.
  std::reverse(std::begin(expected_strings), std::end(expected_strings));
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that when a duplicate is copied, the duplicate shows up in the proper
// order and that the older version is still returned.
TEST_F(ClipboardHistoryTest, DuplicatePrecedesPreviousRecord) {
  // Input holds a unique string sandwiched by a copy.
  std::vector<base::string16> input_strings{
      base::UTF8ToUTF16("test1"), base::UTF8ToUTF16("test2"),
      base::UTF8ToUTF16("test1"), base::UTF8ToUTF16("test3")};
  // The result should be a reversal of the copied elements. When a duplicate
  // is copied, history will show all versions of the recent duplicate.
  std::vector<base::string16> expected_strings{
      base::UTF8ToUTF16("test3"), base::UTF8ToUTF16("test1"),
      base::UTF8ToUTF16("test2"), base::UTF8ToUTF16("test1")};

  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that nothing is saved after history is cleared.
TEST_F(ClipboardHistoryTest, ClearHistoryBasic) {
  // Input holds a unique string sandwhiched by a copy.
  std::vector<base::string16> input_strings{base::UTF8ToUTF16("test1"),
                                            base::UTF8ToUTF16("test2"),
                                            base::UTF8ToUTF16("test1")};
  // The result should be a reversal of the last two elements. When a duplicate
  // is copied, history will show the most recent version of that duplicate.
  std::vector<base::string16> expected_strings{};

  for (const auto& input_string : input_strings) {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(input_string);
  }

  clipboard_history()->Clear();
  EnsureTextHistory(expected_strings);
}

// Tests that the limit of clipboard history is respected.
TEST_F(ClipboardHistoryTest, HistoryLimit) {
  std::vector<base::string16> input_strings{
      base::UTF8ToUTF16("test1"), base::UTF8ToUTF16("test2"),
      base::UTF8ToUTF16("test3"), base::UTF8ToUTF16("test4"),
      base::UTF8ToUTF16("test5"), base::UTF8ToUTF16("test6")};

  // The result should be a reversal of the last five elements.
  std::vector<base::string16> expected_strings{input_strings.begin() + 1,
                                               input_strings.end()};
  std::reverse(expected_strings.begin(), expected_strings.end());
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that pausing clipboard history results in no history collected.
TEST_F(ClipboardHistoryTest, PauseHistory) {
  std::vector<base::string16> input_strings{base::UTF8ToUTF16("test1"),
                                            base::UTF8ToUTF16("test2"),
                                            base::UTF8ToUTF16("test1")};
  // Because history is paused, there should be nothing stored.
  std::vector<base::string16> expected_strings{};

  ClipboardHistory::ScopedPause scoped_pause(clipboard_history());
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that bitmaps are recorded in clipboard history.
TEST_F(ClipboardHistoryTest, BasicBitmap) {
  SkBitmap test_bitmap;
  test_bitmap.allocN32Pixels(3, 2);
  test_bitmap.eraseARGB(255, 0, 255, 0);
  std::vector<SkBitmap> input_bitmaps{test_bitmap};
  std::vector<SkBitmap> expected_bitmaps{test_bitmap};

  WriteAndEnsureBitmapHistory(input_bitmaps, expected_bitmaps);
}

// Tests that duplicate bitmaps show up in history in most-recent order.
TEST_F(ClipboardHistoryTest, DuplicateBitmap) {
  SkBitmap test_bitmap_1;
  test_bitmap_1.allocN32Pixels(3, 2);
  test_bitmap_1.eraseARGB(255, 0, 255, 0);
  SkBitmap test_bitmap_2;
  test_bitmap_2.allocN32Pixels(3, 2);
  test_bitmap_2.eraseARGB(0, 255, 0, 0);

  std::vector<SkBitmap> input_bitmaps{test_bitmap_1, test_bitmap_2,
                                      test_bitmap_1};
  std::vector<SkBitmap> expected_bitmaps = input_bitmaps;
  WriteAndEnsureBitmapHistory(input_bitmaps, expected_bitmaps);
}

// Tests that unrecognized custom data is omitted from clipboard history.
TEST_F(ClipboardHistoryTest, BasicCustomData) {
  const std::unordered_map<base::string16, base::string16> input_data = {
      {base::UTF8ToUTF16("custom-format-1"),
       base::UTF8ToUTF16("custom-data-1")},
      {base::UTF8ToUTF16("custom-format-2"),
       base::UTF8ToUTF16("custom-data-2")}};

  // Custom data which is not recognized is omitted from history.
  WriteAndEnsureCustomDataHistory(input_data, /*expected_data=*/{});
}

// Tests that file system data is recorded in clipboard history.
TEST_F(ClipboardHistoryTest, BasicFileSystemData) {
  const std::unordered_map<base::string16, base::string16> input_data = {
      {base::UTF8ToUTF16("fs/sources"),
       base::UTF8ToUTF16("/path/to/My%20File.txt")}};

  const std::unordered_map<base::string16, base::string16> expected_data =
      input_data;

  WriteAndEnsureCustomDataHistory(input_data, expected_data);
}

}  // namespace ash
