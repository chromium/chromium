// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history.h"

#include <list>
#include <memory>
#include <unordered_map>

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/scoped_clipboard_history_pause_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/clipboard_util.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

namespace ash {

class ClipboardHistoryTest : public AshTestBase {
 public:
  ClipboardHistoryTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ClipboardHistoryTest(const ClipboardHistoryTest&) = delete;
  ClipboardHistoryTest& operator=(const ClipboardHistoryTest&) = delete;
  ~ClipboardHistoryTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    clipboard_history_ = const_cast<ClipboardHistory*>(
        Shell::Get()->clipboard_history_controller()->history());
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());
  }

  const std::list<ClipboardHistoryItem>& GetClipboardHistoryItems() {
    return clipboard_history_->GetItems();
  }

  ui::test::EventGenerator* GetEventGenerator() {
    return event_generator_.get();
  }

  // Writes |input_strings| to the clipboard buffer and ensures that
  // |expected_strings| are retained in history. If |in_same_sequence| is true,
  // writes to the buffer will be performed in the same task sequence.
  void WriteAndEnsureTextHistory(
      const std::vector<std::u16string>& input_strings,
      const std::vector<std::u16string>& expected_strings,
      bool in_same_sequence = false) {
    for (const auto& input_string : input_strings) {
      {
        ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
        scw.WriteText(input_string);
      }
      if (!in_same_sequence) {
        base::RunLoop().RunUntilIdle();
      }
    }
    if (in_same_sequence) {
      base::RunLoop().RunUntilIdle();
    }
    EnsureTextHistory(expected_strings);
  }

  void EnsureTextHistory(const std::vector<std::u16string>& expected_strings) {
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
      // The PNG should not have yet been encoded.
      const auto& maybe_png = item.data().maybe_png();
      EXPECT_FALSE(maybe_png.has_value());

      auto maybe_bitmap = item.data().GetBitmapIfPngNotEncoded();
      EXPECT_TRUE(maybe_bitmap.has_value());
      EXPECT_TRUE(gfx::BitmapsAreEqual(
          expected_bitmaps[expected_bitmaps_index++], maybe_bitmap.value()));
    }
  }

  // Writes |input_data| to the clipboard buffer and ensures that
  // |expected_data| is retained in history. After writing to the buffer, the
  // current task sequence is run to idle to simulate real world behavior.
  void WriteAndEnsureCustomDataHistory(
      const std::unordered_map<std::u16string, std::u16string>& input_data,
      const std::unordered_map<std::u16string, std::u16string>& expected_data) {
    base::Pickle input_data_pickle;
    ui::WriteCustomDataToPickle(input_data, &input_data_pickle);

    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WritePickledData(input_data_pickle,
                           ui::ClipboardFormatType::DataTransferCustomType());
    }
    base::RunLoop().RunUntilIdle();

    const std::list<ClipboardHistoryItem> items = GetClipboardHistoryItems();
    EXPECT_EQ(expected_data.empty() ? 0u : 1u, items.size());

    if (expected_data.empty()) {
      return;
    }

    std::optional<std::unordered_map<std::u16string, std::u16string>>
        actual_data = ui::ReadCustomDataIntoMap(base::as_bytes(
            base::span(items.front().data().GetDataTransferCustomData())));

    EXPECT_EQ(expected_data, actual_data);
  }

  ClipboardHistory* clipboard_history() { return clipboard_history_; }

 private:
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  // Owned by ClipboardHistoryControllerImpl.
  raw_ptr<ClipboardHistory, DanglingUntriaged> clipboard_history_ = nullptr;
};

// Tests that with nothing copied, nothing is shown.
TEST_F(ClipboardHistoryTest, NothingCopiedNothingSaved) {
  // When nothing is copied, nothing should be saved.
  WriteAndEnsureTextHistory(/*input_strings=*/{},
                            /*expected_strings=*/{});
}

// Tests that if one thing is copied, one thing is saved.
TEST_F(ClipboardHistoryTest, OneThingCopiedOneThingSaved) {
  std::vector<std::u16string> input_strings{u"test"};
  std::vector<std::u16string> expected_strings = input_strings;

  // Test that only one string is in history.
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that if the same (non bitmap) thing is copied, only one of the
// duplicates is in the list.
TEST_F(ClipboardHistoryTest, DuplicateBasic) {
  std::vector<std::u16string> input_strings{u"test", u"test"};
  std::vector<std::u16string> expected_strings{u"test"};

  // Test that both things are saved.
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that if multiple things are copied in the same task sequence, only the
// most recent thing is saved.
TEST_F(ClipboardHistoryTest, InSameSequenceBasic) {
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test3"};
  // Because |input_strings| will be copied in the same task sequence, history
  // should only retain the most recent thing.
  std::vector<std::u16string> expected_strings{u"test3"};

  // Test that only the most recent thing is saved.
  WriteAndEnsureTextHistory(input_strings, expected_strings,
                            /*in_same_sequence=*/true);
}

// Tests the ordering of history is in reverse chronological order.
TEST_F(ClipboardHistoryTest, HistoryIsReverseChronological) {
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test3",
                                            u"test4"};
  std::vector<std::u16string> expected_strings = input_strings;
  // Reverse the vector, history should match this ordering.
  std::reverse(std::begin(expected_strings), std::end(expected_strings));
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that when a duplicate is copied, the existing duplicate item moves up
// to the front of the clipboard history.
TEST_F(ClipboardHistoryTest, DuplicatePrecedesPreviousRecord) {
  // Input holds four strings, two of which are the same.
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test1",
                                            u"test3"};
  // The result should be a reversal of the copied elements. When a duplicate
  // is copied, history will have that item moved to the front instead of adding
  // a new item.
  std::vector<std::u16string> expected_strings{u"test3", u"test1", u"test2"};

  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that nothing is saved after history is cleared.
TEST_F(ClipboardHistoryTest, ClearingClipboardHistoryClearsClipboard) {
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test1"};
  // The result should be empty due to history being cleared.
  std::vector<std::u16string> expected_strings{};

  for (const auto& input_string : input_strings) {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(input_string);
  }

  clipboard_history()->Clear();
  EnsureTextHistory(expected_strings);

  // The clipboard should also be empty after the clipboard history is cleared.
  auto* const clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ASSERT_TRUE(clipboard);
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  ASSERT_FALSE(clipboard->GetClipboardData(&data_dst));
}

// Tests that clipboard history is cleared when the clipboard is cleared.
TEST_F(ClipboardHistoryTest, ClearingClipboardClearsClipboardHistory) {
  std::vector<std::u16string> input_strings{u"test1", u"test2"};

  std::vector<std::u16string> expected_strings_before_clear{u"test2", u"test1"};
  std::vector<std::u16string> expected_strings_after_clear{};

  for (const auto& input_string : input_strings) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(input_string);
    }
    base::RunLoop().RunUntilIdle();
  }

  EnsureTextHistory(expected_strings_before_clear);

  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);

  // The clipboard should be empty after being cleared.
  auto* const clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ASSERT_TRUE(clipboard);
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  ASSERT_FALSE(clipboard->GetClipboardData(&data_dst));

  // The clipboard history should also be empty when the clipboard is cleared.
  EnsureTextHistory(expected_strings_after_clear);
}

// Tests that there is no crash when an empty clipboard is cleared with empty
// clipboard history.
TEST_F(ClipboardHistoryTest, ClearEmptyClipboard) {
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
}

// Tests that there is no crash when an empty clipboard history is cleared with
// empty clipboard.
TEST_F(ClipboardHistoryTest, ClearEmptyClipboardHistory) {
  clipboard_history()->Clear();
}

// Tests that the limit of clipboard history is respected.
TEST_F(ClipboardHistoryTest, HistoryLimit) {
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test3",
                                            u"test4", u"test5", u"test6"};

  // The result should be a reversal of the last five elements.
  std::vector<std::u16string> expected_strings{input_strings.begin() + 1,
                                               input_strings.end()};
  std::reverse(expected_strings.begin(), expected_strings.end());
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that pausing clipboard history results in no history collected.
TEST_F(ClipboardHistoryTest, PauseHistoryBasic) {
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test1"};
  // Because history is paused, there should be nothing stored.
  std::vector<std::u16string> expected_strings{};

  ScopedClipboardHistoryPauseImpl scoped_pause(clipboard_history());
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that pausing clipboard history with the `kAllowReorderOnPaste` pause
// behavior allows clipboard history to be modified, but clipboard data changes
// received during the pause are not recorded as copy operations.
TEST_F(ClipboardHistoryTest, PauseHistoryAllowReorders) {
  std::vector<std::u16string> input_strings{u"test1", u"test2"};
  std::vector<std::u16string> input_string1{u"test1"};
  std::vector<std::u16string> expected_strings_initial{u"test2", u"test1"};
  std::vector<std::u16string> expected_strings_reordered = input_strings;

  // Populate clipboard history to simulate paste-based reorders.
  WriteAndEnsureTextHistory(input_strings, expected_strings_initial);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);

  ScopedClipboardHistoryPauseImpl scoped_pause(
      clipboard_history(),
      clipboard_history_util::PauseBehavior::kAllowReorderOnPaste);
  WriteAndEnsureTextHistory(input_string1, expected_strings_reordered);
  // Clipboard history modifications made during a reorder-on-paste operation
  // should not count as copies (or pastes). A reorder is not a user action.
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);
}

// Tests that when already-paused clipboard history is paused again with a
// different behavior, the newest behavior overrides all others for the duration
// of the pause's lifetime.
TEST_F(ClipboardHistoryTest, PauseHistoryNested) {
  std::vector<std::u16string> input_strings{u"test1", u"test2"};
  std::vector<std::u16string> input_string1{u"test1"};
  std::vector<std::u16string> input_string2{u"test2"};
  std::vector<std::u16string> input_string3{u"test3"};
  std::vector<std::u16string> expected_strings_initial{u"test2", u"test1"};
  std::vector<std::u16string> expected_strings_reordered1 = input_strings;
  std::vector<std::u16string> expected_strings_reordered2 =
      expected_strings_initial;

  // Populate clipboard history to simulate paste-based reorders.
  WriteAndEnsureTextHistory(input_strings, expected_strings_initial);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);

  // By default, pausing prevents clipboard history modifications.
  ScopedClipboardHistoryPauseImpl scoped_pause_default_1(
      clipboard_history(), clipboard_history_util::PauseBehavior::kDefault);
  WriteAndEnsureTextHistory(input_string3, expected_strings_initial);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);

  {
    // When allowing paste-based reorders, clipboard history should be modified.
    // Ensure that nesting pauses causes earlier pause behavior to be
    // overridden.
    ScopedClipboardHistoryPauseImpl scoped_pause_allow_reorders(
        clipboard_history(),
        clipboard_history_util::PauseBehavior::kAllowReorderOnPaste);
    WriteAndEnsureTextHistory(input_string1, expected_strings_reordered1);
    // Clipboard history modifications made during a reorder-on-paste operation
    // should not count as copies (or pastes). A reorder is not a user action.
    histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);

    {
      // Test that the newest behavior always applies, regardless of what order
      // behaviors were overridden.
      ScopedClipboardHistoryPauseImpl scoped_pause_default_2(
          clipboard_history(), clipboard_history_util::PauseBehavior::kDefault);
      WriteAndEnsureTextHistory(input_string3, expected_strings_reordered1);
      histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);
    }

    // Test that the previous behavior is restored when the newest pause goes
    // out of scope.
    WriteAndEnsureTextHistory(input_string2, expected_strings_reordered2);
    histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);
  }

  // Test that the previous behavior is restored when the newest pause goes out
  // of scope, regardless of what order behaviors were overridden.
  WriteAndEnsureTextHistory(input_string3, expected_strings_reordered2);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);
}

// Tests that clipboard history pauses do not have to be destroyed in LIFO
// order.
TEST_F(ClipboardHistoryTest, PauseHistoryResumeOutOfOrder) {
  std::vector<std::u16string> input_strings{u"test1", u"test2"};
  std::vector<std::u16string> input_string1{u"test1"};
  std::vector<std::u16string> input_string2{u"test2"};
  std::vector<std::u16string> input_string3{u"test3"};
  std::vector<std::u16string> expected_strings_initial{u"test2", u"test1"};
  std::vector<std::u16string> expected_strings_reordered1 = input_strings;
  std::vector<std::u16string> expected_strings_reordered2 =
      expected_strings_initial;
  std::vector<std::u16string> expected_strings_new_item = {u"test3", u"test2",
                                                           u"test1"};

  // Populate clipboard history to simulate paste-based reorders.
  WriteAndEnsureTextHistory(input_strings, expected_strings_initial);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);

  auto scoped_pause_default = std::make_unique<ScopedClipboardHistoryPauseImpl>(
      clipboard_history(), clipboard_history_util::PauseBehavior::kDefault);
  WriteAndEnsureTextHistory(input_string3, expected_strings_initial);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);

  auto scoped_pause_allow_reorders =
      std::make_unique<ScopedClipboardHistoryPauseImpl>(
          clipboard_history(),
          clipboard_history_util::PauseBehavior::kAllowReorderOnPaste);
  WriteAndEnsureTextHistory(input_string1, expected_strings_reordered1);
  // Clipboard history modifications made during a reorder-on-paste operation
  // should not count as copies (or pastes). A reorder is not a user action.
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);

  // Verify that pauses can be destroyed in non-LIFO order without changing the
  // current pause behavior.
  scoped_pause_default.reset();
  WriteAndEnsureTextHistory(input_string2, expected_strings_reordered2);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 0u);

  // Verify that when all pauses are destroyed, clipboard history is modified as
  // usual.
  base::test::TestFuture<bool> operation_confirmed_future;
  Shell::Get()
      ->clipboard_history_controller()
      ->set_confirmed_operation_callback_for_test(
          operation_confirmed_future.GetRepeatingCallback());
  scoped_pause_allow_reorders.reset();
  WriteAndEnsureTextHistory(input_string3, expected_strings_new_item);
  // Since clipboard history is not paused in any way, data being written to the
  // clipboard is interpreted as a copy operation.
  EXPECT_EQ(operation_confirmed_future.Take(), true);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.Operation", 1u);
}

// Tests that bitmaps are recorded in clipboard history.
TEST_F(ClipboardHistoryTest, BasicBitmap) {
  SkBitmap test_bitmap = gfx::test::CreateBitmap(3, 2);
  std::vector<SkBitmap> input_bitmaps{test_bitmap};
  std::vector<SkBitmap> expected_bitmaps{test_bitmap};

  WriteAndEnsureBitmapHistory(input_bitmaps, expected_bitmaps);
}

// Tests that duplicate bitmaps show up in history as one item placed in
// most-recent order.
TEST_F(ClipboardHistoryTest, DuplicateBitmap) {
  SkBitmap test_bitmap_1 = gfx::test::CreateBitmap(3, 2);
  SkBitmap test_bitmap_2 = gfx::test::CreateBitmap(4, 3);

  std::vector<SkBitmap> input_bitmaps{test_bitmap_1, test_bitmap_2,
                                      test_bitmap_1};
  std::vector<SkBitmap> expected_bitmaps{test_bitmap_1, test_bitmap_2};
  WriteAndEnsureBitmapHistory(input_bitmaps, expected_bitmaps);
}

// Tests that when a duplicate bitmap is written to clipboard history and the
// first bitmap has been encoded to a PNG, the encoded PNG is still set on the
// clipboard history item's data after deduplication.
TEST_F(ClipboardHistoryTest, DuplicateBitmapEncodingPreserved) {
  SkBitmap test_bitmap_1 = gfx::test::CreateBitmap(3, 2);
  SkBitmap test_bitmap_2 = gfx::test::CreateBitmap(4, 3);

  // Write image data to clipboard.
  std::vector<SkBitmap> input_bitmaps{test_bitmap_1, test_bitmap_2};
  std::vector<SkBitmap> expected_bitmaps{test_bitmap_2, test_bitmap_1};
  WriteAndEnsureBitmapHistory(input_bitmaps, expected_bitmaps);

  // Encode the image belonging to the data that will be written again.
  const std::list<ClipboardHistoryItem>& items = GetClipboardHistoryItems();
  ASSERT_EQ(items.size(), 2u);
  const auto& data_to_duplicate = items.back().data();
  const auto original_sequence_number_token =
      data_to_duplicate.sequence_number_token();
  const auto original_timestamp = items.back().time_copied();
  EXPECT_FALSE(data_to_duplicate.maybe_png());
  auto png = ui::clipboard_util::EncodeBitmapToPng(test_bitmap_1);
  data_to_duplicate.SetPngDataAfterEncoding(png);
  EXPECT_TRUE(data_to_duplicate.maybe_png());

  // Write first image to clipboard again after some time passes.
  task_environment()->FastForwardBy(base::Seconds(1));
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteImage(test_bitmap_1);
  }
  base::RunLoop().RunUntilIdle();

  // Verify that the encoded image data was preserved while deduplicating data
  // and reordering items in clipboard history.
  ASSERT_EQ(items.size(), 2u);
  EXPECT_GT(items.front().time_copied(), original_timestamp);
  const auto& duplicated_data = items.front().data();
  EXPECT_EQ(duplicated_data, data_to_duplicate);
  EXPECT_NE(duplicated_data.sequence_number_token(),
            original_sequence_number_token);
  ASSERT_TRUE(duplicated_data.maybe_png());
  EXPECT_EQ(*duplicated_data.maybe_png(), png);
}

// Tests that unrecognized custom data is omitted from clipboard history.
TEST_F(ClipboardHistoryTest, BasicCustomData) {
  const std::unordered_map<std::u16string, std::u16string> input_data = {
      {u"custom-format-1", u"custom-data-1"},
      {u"custom-format-2", u"custom-data-2"}};

  // Custom data which is not recognized is omitted from history.
  WriteAndEnsureCustomDataHistory(input_data, /*expected_data=*/{});
}

// Tests that file system data is recorded in clipboard history.
TEST_F(ClipboardHistoryTest, BasicFileSystemData) {
  const std::unordered_map<std::u16string, std::u16string> input_data = {
      {u"fs/sources", u"/path/to/My%20File.txt"}};

  const std::unordered_map<std::u16string, std::u16string> expected_data =
      input_data;

  WriteAndEnsureCustomDataHistory(input_data, expected_data);
}

// Tests that the display format for HTML with no <img> or <table> tags is text.
TEST_F(ClipboardHistoryTest, DisplayFormatForPlainHTML) {
  ui::ClipboardData data;
  data.set_markup_data("plain html with no img or table tags");
  EXPECT_EQ(ClipboardHistoryItem(data).display_format(),
            crosapi::mojom::ClipboardHistoryDisplayFormat::kText);

  data.set_markup_data("<img> </img>");
  EXPECT_EQ(ClipboardHistoryItem(data).display_format(),
            crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml);
}

// Tests that exactly one Ash.ClipboardHistory.ControlToVDelayV2 histogram entry
// is recorded every time Ctrl is pressed as part of a Ctrl+V paste sequence and
// that exactly one Ash.ClipboardHistory.ControlVHeldTime histogram entry is
// recorded every time V is pressed as part of a Ctrl+V paste sequence.
TEST_F(ClipboardHistoryTest, RecordControlVMetrics) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelayV2",
                                    0u);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlVHeldTime",
                                    0u);

  auto* const event_generator = GetEventGenerator();

  // Press Ctrl+V and end the paste by releasing V. One entry should be recorded
  // for each histogram.
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  PressAndReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelayV2",
                                    1u);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlVHeldTime",
                                    1u);

  // Press V again, injecting an extra press to simulate a keyboard auto-repeat
  // from holding V down. Neither of these V presses is the first in the paste
  // sequence, so no entry should be recorded for the ControlToVDelayV2
  // histogram.
  event_generator->PressKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  event_generator->PressKey(ui::VKEY_V, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelayV2",
                                    1u);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlVHeldTime",
                                    1u);

  // Release Ctrl to end the paste sequence. An entry should be recorded for the
  // ControlVHeldTime histogram.
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelayV2",
                                    1u);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlVHeldTime",
                                    2u);

  // Release V so that no more keys are pressed. No entry should be recorded for
  // the ControlVHeldTime histogram, because the paste sequence already ended.
  event_generator->ReleaseKey(ui::VKEY_V, ui::EF_NONE);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelayV2",
                                    1u);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlVHeldTime",
                                    2u);

  // Press Ctrl+V and end the paste by pressing a key other than Ctrl or V. One
  // entry should be recorded for each histogram.
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_X, ui::EF_CONTROL_DOWN);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelayV2",
                                    2u);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlVHeldTime",
                                    3u);

  // Release V and Ctrl so that no more keys are pressed. No histogram entries
  // should be recorded.
  event_generator->ReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelayV2",
                                    2u);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlVHeldTime",
                                    3u);

  // Hold Shift while pressing and releasing Ctrl+V. No histogram entries should
  // be recorded.
  event_generator->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_SHIFT_DOWN);
  event_generator->ReleaseKey(ui::VKEY_SHIFT, ui::EF_NONE);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelayV2",
                                    2u);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlVHeldTime",
                                    3u);

  // Press Ctrl, then press and release a key other than V, then press and
  // release V. One entry should be recorded for each histogram.
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  PressAndReleaseKey(ui::VKEY_X, ui::EF_CONTROL_DOWN);

  // Allow some time between the arbitrary key press and the V key press, during
  // which time Ctrl is pressed again.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_IS_REPEAT);
  task_environment()->FastForwardBy(base::Milliseconds(100));

  PressAndReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelayV2",
                                    3u);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlVHeldTime",
                                    4u);

  // The ControlToVDelayV2 histogram should have one recorded sample of 200ms
  // from the last Ctrl+V; the other recorded samples should all be 0ms. 200ms
  // comes from the delay between the last paste sequence's first Ctrl press and
  // its first V press (the sequence's second Ctrl press between the 100ms
  // pauses should not affect metrics).
  histogram_tester.ExpectTimeBucketCount(
      "Ash.ClipboardHistory.ControlToVDelayV2", base::Milliseconds(0), 2);
  histogram_tester.ExpectTimeBucketCount(
      "Ash.ClipboardHistory.ControlToVDelayV2", base::Milliseconds(100), 0);
  histogram_tester.ExpectTimeBucketCount(
      "Ash.ClipboardHistory.ControlToVDelayV2", base::Milliseconds(200), 1);
}

}  // namespace ash
