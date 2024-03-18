// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include <string>

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_test_util.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/emoji/emoji_panel_helper.h"
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

class PickerControllerTest : public AshTestBase {
 public:
  PickerControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

// A PickerClient implementation used for testing.
// Automatically sets itself as the client when it's created, and unsets itself
// when it's destroyed.
class TestPickerClient : public PickerClient {
 public:
  explicit TestPickerClient(PickerController* controller)
      : controller_(controller) {
    controller_->SetClient(this);
  }
  ~TestPickerClient() override { controller_->SetClient(nullptr); }

  std::unique_ptr<ash::AshWebView> CreateWebView(
      const ash::AshWebView::InitParams& params) override {
    return web_view_factory_.Create(params);
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override {
    return base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
  }

  void FetchGifSearch(const std::string& query,
                      FetchGifsCallback callback) override {}
  void StopGifSearch() override {}
  void StartCrosSearch(const std::u16string& query,
                       std::optional<PickerCategory> category,
                       CrosSearchResultsCallback callback) override {}
  void StopCrosQuery() override {}

 private:
  TestAshWebViewFactory web_view_factory_;
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

  controller.InsertResultOnNextFocus(
      PickerSearchResult::Clipboard(*clipboard_item_id));
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
  std::optional<ui::EmojiPickerCategory> emoji_category;
  ui::SetShowEmojiKeyboardCallback(base::BindLambdaForTesting(
      [&emoji_category](ui::EmojiPickerCategory category) {
        emoji_category = category;
      }));

  controller.ShowEmojiPicker(ui::EmojiPickerCategory::kSymbols);

  EXPECT_EQ(emoji_category, ui::EmojiPickerCategory::kSymbols);
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

}  // namespace
}  // namespace ash
