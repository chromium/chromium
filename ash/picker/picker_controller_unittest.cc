// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include <string>

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_test_util.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/picker/mock_picker_client.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

bool CopyTextToClipboard() {
  base::test::TestFuture<bool> copy_confirmed_future;
  Shell::Get()
      ->clipboard_history_controller()
      ->set_confirmed_operation_callback_for_test(
          copy_confirmed_future.GetRepeatingCallback());
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"test");
  }
  return copy_confirmed_future.Take();
}

std::optional<base::UnguessableToken> GetFirstClipboardItemId() {
  base::test::TestFuture<std::vector<ClipboardHistoryItem>> future;
  auto* controller = ClipboardHistoryController::Get();
  controller->GetHistoryValues(future.GetCallback());

  std::vector<ClipboardHistoryItem> items = future.Take();
  return items.empty() ? std::nullopt : std::make_optional(items.front().id());
}

class ClipboardPasteWaiter : public ClipboardHistoryController::Observer {
 public:
  ClipboardPasteWaiter() {
    observation_.Observe(ClipboardHistoryController::Get());
  }

  void Wait() {
    if (observation_.IsObserving()) {
      run_loop_.Run();
    }
  }

  // ClipboardHistoryController::Observer:
  void OnClipboardHistoryPasted() override {
    observation_.Reset();
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<ClipboardHistoryController,
                          ClipboardHistoryController::Observer>
      observation_{this};
};

input_method::ImeKeyboard* GetImeKeyboard() {
  auto* input_method_manager = input_method::InputMethodManager::Get();
  return input_method_manager ? input_method_manager->GetImeKeyboard()
                              : nullptr;
}

class PickerControllerTest : public AshTestBase {
 public:
  PickerControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

// A PickerClient implementation used for testing.
// Automatically sets itself as the client when it's created, and unsets itself
// when it's destroyed.
class TestPickerClient : public MockPickerClient {
 public:
  explicit TestPickerClient(PickerController* controller)
      : controller_(controller) {
    controller_->SetClient(this);
    // Set default behaviours. These can be overridden with `WillOnce` and
    // `WillRepeatedly`.
    ON_CALL(*this, GetSharedURLLoaderFactory)
        .WillByDefault(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>);
  }
  ~TestPickerClient() override { controller_->SetClient(nullptr); }

 private:
  raw_ptr<PickerController> controller_ = nullptr;
};

TEST_F(PickerControllerTest, ToggleWidgetShowsWidgetIfClosed) {
  PickerController controller;
  TestPickerClient client(&controller);

  controller.ToggleWidget();

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetClosesWidgetIfOpen) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller.widget_for_testing());

  controller.ToggleWidget();

  widget_destroyed_waiter.Wait();
  EXPECT_FALSE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetShowsWidgetIfOpenedThenClosed) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller.widget_for_testing());
  controller.ToggleWidget();
  widget_destroyed_waiter.Wait();

  controller.ToggleWidget();

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, SetClientToNullKeepsWidget) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();

  controller.SetClient(nullptr);

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ShowWidgetRecordsInputReadyLatency) {
  base::HistogramTester histogram;
  PickerController controller;
  TestPickerClient client(&controller);

  controller.ToggleWidget(base::TimeTicks::Now());
  views::test::WidgetVisibleWaiter widget_visible_waiter(
      controller.widget_for_testing());
  widget_visible_waiter.Wait();

  histogram.ExpectTotalCount("Ash.Picker.Session.InputReadyLatency", 1);
}

TEST_F(PickerControllerTest, InsertResultDoesNothingWhenWidgetIsClosed) {
  PickerController controller;
  TestPickerClient client(&controller);
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller.InsertResultOnNextFocus(PickerSearchResult::Text(u"abc"));
  ui::FakeTextInputClient input_field(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&input_field);
  absl::Cleanup focused_input_field_reset = [input_method] {
    // Reset the input field since it will be destroyed before `input_method`.
    input_method->SetFocusedTextInputClient(nullptr);
  };

  EXPECT_EQ(input_field.text(), u"");
}

TEST_F(PickerControllerTest, InsertTextResultInsertsIntoInputFieldAfterFocus) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller.InsertResultOnNextFocus(PickerSearchResult::Text(u"abc"));
  controller.widget_for_testing()->CloseNow();
  ui::FakeTextInputClient input_field(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&input_field);
  absl::Cleanup focused_input_field_reset = [input_method] {
    // Reset the input field since it will be destroyed before `input_method`.
    input_method->SetFocusedTextInputClient(nullptr);
  };

  EXPECT_EQ(input_field.text(), u"abc");
}

TEST_F(PickerControllerTest,
       InsertClipboardResultPastesIntoInputFieldAfterFocus) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  ASSERT_TRUE(CopyTextToClipboard());
  std::optional<base::UnguessableToken> clipboard_item_id =
      GetFirstClipboardItemId();
  ASSERT_TRUE(clipboard_item_id.has_value());

  controller.InsertResultOnNextFocus(PickerSearchResult::Clipboard(
      *clipboard_item_id,
      PickerSearchResult::ClipboardData::DisplayFormat::kText,
      /*display_text=*/u"", /*display_image=*/{}));
  controller.widget_for_testing()->CloseNow();
  ClipboardPasteWaiter waiter;
  // Create a new to focus on.
  auto new_widget = CreateFramelessTestWidget();

  waiter.Wait();
}

TEST_F(PickerControllerTest, InsertGifResultInsertsIntoInputFieldAfterFocus) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller.InsertResultOnNextFocus(PickerSearchResult::Gif(
      GURL("http://foo.com/fake_preview.gif"),
      GURL("http://foo.com/fake_preview_image.png"), gfx::Size(),
      GURL("http://foo.com/fake.gif"), gfx::Size(),
      /*content_description=*/u""));
  controller.widget_for_testing()->CloseNow();
  ui::FakeTextInputClient input_field(
      input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.last_inserted_image_url(),
            GURL("http://foo.com/fake.gif"));
}

TEST_F(PickerControllerTest,
       InsertUnsupportedImageResultTimeoutCopiesToClipboard) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller.InsertResultOnNextFocus(PickerSearchResult::Gif(
      /*preview_url=*/GURL("http://foo.com/preview"),
      /*preview_image_url=*/GURL(), gfx::Size(30, 20),
      /*full_url=*/GURL("http://foo.com"), gfx::Size(60, 40),
      /*content_description=*/u"a gif"));
  controller.widget_for_testing()->CloseNow();
  ui::FakeTextInputClient input_field(
      input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});
  input_method->SetFocusedTextInputClient(&input_field);
  task_environment()->FastForwardBy(PickerController::kInsertMediaTimeout);

  EXPECT_EQ(
      ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
      uR"html(<img src="http://foo.com/" referrerpolicy="no-referrer" alt="a gif" width="60" height="40"/>)html");
  EXPECT_TRUE(
      ash::ToastManager::Get()->IsToastShown("picker_copy_to_clipboard"));
}

TEST_F(PickerControllerTest,
       InsertBrowsingHistoryResultInsertsIntoInputFieldAfterFocus) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller.InsertResultOnNextFocus(PickerSearchResult::BrowsingHistory(
      GURL("http://foo.com"), u"Foo", ui::ImageModel{}));
  controller.widget_for_testing()->CloseNow();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"http://foo.com/");
}

TEST_F(PickerControllerTest, ShowEmojiPickerCallsEmojiPanelCallback) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  base::test::TestFuture<ui::EmojiPickerCategory, ui::EmojiPickerFocusBehavior>
      future;
  ui::SetShowEmojiKeyboardCallback(future.GetRepeatingCallback());

  controller.ShowEmojiPicker(ui::EmojiPickerCategory::kSymbols);

  const auto& [category, focus_behavior] = future.Get();
  EXPECT_EQ(category, ui::EmojiPickerCategory::kSymbols);
  EXPECT_EQ(focus_behavior, ui::EmojiPickerFocusBehavior::kAlwaysShow);
}

TEST_F(PickerControllerTest, SetCapsLockEnabledToTrueTurnsOnCapsLock) {
  PickerController controller;
  TestPickerClient client(&controller);

  controller.SetCapsLockEnabled(true);

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_TRUE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(PickerControllerTest, SetCapsLockEnabledToFalseTurnsOffCapsLock) {
  PickerController controller;
  TestPickerClient client(&controller);

  controller.SetCapsLockEnabled(false);

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_FALSE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(PickerControllerTest, ShowingAndClosingWidgetRecordsUsageMetrics) {
  base::HistogramTester histogram_tester;
  PickerController controller;
  TestPickerClient client(&controller);

  // Show the widget twice.
  controller.ToggleWidget();
  task_environment()->FastForwardBy(base::Seconds(1));
  controller.widget_for_testing()->CloseNow();
  task_environment()->FastForwardBy(base::Seconds(2));
  controller.ToggleWidget();
  task_environment()->FastForwardBy(base::Seconds(3));
  controller.widget_for_testing()->CloseNow();
  task_environment()->FastForwardBy(base::Seconds(4));

  histogram_tester.ExpectBucketCount(
      "ChromeOS.FeatureUsage.Picker",
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      2);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.FeatureUsage.Picker",
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure),
      0);
  histogram_tester.ExpectTimeBucketCount("ChromeOS.FeatureUsage.Picker.Usetime",
                                         base::Seconds(1), 1);
  histogram_tester.ExpectTimeBucketCount("ChromeOS.FeatureUsage.Picker.Usetime",
                                         base::Seconds(3), 1);
}

TEST_F(PickerControllerTest, GetUpperCaseSelectedText) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"abc", gfx::Range(0, 3));
  PickerController controller;
  TestPickerClient client(&controller);

  controller.ToggleWidget();
  controller.TransformSelectedText(PickerCategory::kUpperCase);
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"ABC");
}

TEST_F(PickerControllerTest, GetLowerCaseSelectedText) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"XYZ", gfx::Range(0, 3));
  PickerController controller;
  TestPickerClient client(&controller);

  controller.ToggleWidget();
  controller.TransformSelectedText(PickerCategory::kLowerCase);
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"xyz");
}

TEST_F(PickerControllerTest, GetTitleCaseSelectedText) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"how are you", gfx::Range(0, 11));
  PickerController controller;
  TestPickerClient client(&controller);

  controller.ToggleWidget();
  controller.TransformSelectedText(PickerCategory::kTitleCase);
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"How Are You");
}

TEST_F(PickerControllerTest, GetSentenceCaseSelectedText) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"how are you? fine. thanks!  ok",
                                  gfx::Range(0, 30));
  PickerController controller;
  TestPickerClient client(&controller);

  controller.ToggleWidget();
  controller.TransformSelectedText(PickerCategory::kSentenceCase);
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"How are you? Fine. Thanks!  Ok");
}
}  // namespace
}  // namespace ash
