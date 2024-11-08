// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/quick_insert/metrics/quick_insert_session_metrics.h"
#include "ash/quick_insert/mock_quick_insert_client.h"
#include "ash/quick_insert/model/quick_insert_action_type.h"
#include "ash/quick_insert/model/quick_insert_caps_lock_position.h"
#include "ash/quick_insert/model/quick_insert_model.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_test_util.h"
#include "ash/quick_insert/views/quick_insert_feature_tour.h"
#include "ash/quick_insert/views/quick_insert_search_bar_textfield.h"
#include "ash/quick_insert/views/quick_insert_search_field_view.h"
#include "ash/quick_insert/views/quick_insert_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/test/test_structured_metrics_recorder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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
#include "ui/base/ime/text_input_type.h"
#include "ui/base/models/image_model.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/focus_controller.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;
using ::testing::VariantWith;

namespace cros_events = metrics::structured::events::v2::cr_os_events;

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

class MockNewWindowDelegate : public TestNewWindowDelegate {
 public:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
  MOCK_METHOD(void, OpenFile, (const base::FilePath& file_path), (override));
};

// A QuickInsertClient implementation used for testing.
// Automatically sets itself as the client when it's created, and unsets itself
// when it's destroyed.
class TestQuickInsertClient : public MockQuickInsertClient {
 public:
  TestQuickInsertClient(QuickInsertController* controller,
                        sync_preferences::TestingPrefServiceSyncable* prefs)
      : controller_(controller), prefs_(prefs) {
    controller_->SetClient(this);
    // Set default behaviours. These can be overridden with `WillOnce` and
    // `WillRepeatedly`.
    ON_CALL(*this, GetSharedURLLoaderFactory)
        .WillByDefault(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>);
    ON_CALL(*this, GetPrefs).WillByDefault(Return(prefs_));
  }
  ~TestQuickInsertClient() override { controller_->SetClient(nullptr); }

  PrefRegistrySimple* registry() { return prefs_->registry(); }

 private:
  raw_ptr<QuickInsertController> controller_ = nullptr;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_ = nullptr;
};

class QuickInsertControllerTest : public AshTestBase {
 public:
  QuickInsertControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<QuickInsertController>();
    client_ = std::make_unique<NiceMock<TestQuickInsertClient>>(
        controller_.get(), &prefs_);
    prefs_.registry()->RegisterDictionaryPref(prefs::kEmojiPickerHistory);
    PickerSessionMetrics::RegisterProfilePrefs(prefs_.registry());
    metrics_recorder_ =
        std::make_unique<metrics::structured::TestStructuredMetricsRecorder>();
    metrics_recorder_->Initialize();
  }

  void TearDown() override {
    client_.reset();
    controller_.reset();
    metrics_recorder_.reset();
    AshTestBase::TearDown();
  }

  MockNewWindowDelegate& mock_new_window_delegate() {
    return new_window_delegate_;
  }

  QuickInsertController& controller() { return *controller_; }

  NiceMock<TestQuickInsertClient>& client() { return *client_; }

  sync_preferences::TestingPrefServiceSyncable& prefs() { return prefs_; }

  metrics::structured::TestStructuredMetricsRecorder& metrics_recorder() {
    return *metrics_recorder_;
  }

 private:
  MockNewWindowDelegate new_window_delegate_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<QuickInsertController> controller_;
  std::unique_ptr<NiceMock<TestQuickInsertClient>> client_;
  std::unique_ptr<metrics::structured::TestStructuredMetricsRecorder>
      metrics_recorder_;
};

TEST_F(QuickInsertControllerTest, ToggleWidgetShowsWidgetIfClosed) {
  controller().ToggleWidget();

  EXPECT_TRUE(controller().widget_for_testing());
}

TEST_F(QuickInsertControllerTest,
       ToggleWidgetInPasswordFieldTogglesCapslockAndShowsBubbleForAShortTime) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_PASSWORD});
  input_method->SetFocusedTextInputClient(&input_field);

  controller().ToggleWidget();
  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);

  EXPECT_FALSE(controller().widget_for_testing());
  EXPECT_TRUE(controller()
                  .caps_lock_bubble_controller_for_testing()
                  .bubble_view_for_testing());
  EXPECT_TRUE(ime_keyboard->IsCapsLockEnabled());

  task_environment()->FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(controller()
                   .caps_lock_bubble_controller_for_testing()
                   .bubble_view_for_testing());
}

TEST_F(QuickInsertControllerTest, TogglingWidgetRecordsStartSessionMetrics) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"abcd", gfx::Range(1, 4));
  input_method->SetFocusedTextInputClient(&input_field);

  controller().ToggleWidget();

  cros_events::Picker_StartSession expected_event;
  expected_event
      .SetInputFieldType(cros_events::PickerInputFieldType::PLAIN_TEXT)
      .SetSelectionLength(3);
  EXPECT_THAT(
      metrics_recorder().GetEvents(),
      ElementsAre(AllOf(
          Property("event name", &metrics::structured::Event::event_name,
                   Eq(expected_event.event_name())),
          Property("metric values", &metrics::structured::Event::metric_values,
                   Eq(std::ref(expected_event.metric_values()))))));
}

TEST_F(QuickInsertControllerTest, ToggleWidgetClosesWidgetIfOpen) {
  controller().ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());

  controller().ToggleWidget();

  widget_destroyed_waiter.Wait();
  EXPECT_FALSE(controller().widget_for_testing());
}

TEST_F(QuickInsertControllerTest, ToggleWidgetShowsWidgetIfOpenedThenClosed) {
  controller().ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
  controller().ToggleWidget();
  widget_destroyed_waiter.Wait();

  controller().ToggleWidget();

  EXPECT_TRUE(controller().widget_for_testing());
}

TEST_F(QuickInsertControllerTest, ToggleWidgetShowsFeatureTourForFirstTime) {
  PickerFeatureTour::RegisterProfilePrefs(client().registry());
  controller().ToggleWidget();

  EXPECT_TRUE(controller().feature_tour_for_testing().widget_for_testing());
  EXPECT_FALSE(controller().widget_for_testing());
}

TEST_F(QuickInsertControllerTest,
       ToggleWidgetShowsWidgetAfterCompletingFeatureTourWithNoWindows) {
  wm::FocusController* focus_controller = Shell::Get()->focus_controller();
  ASSERT_EQ(focus_controller->GetActiveWindow(), nullptr);
  ASSERT_EQ(focus_controller->GetFocusedWindow(), nullptr);

  // Show the feature tour.
  PickerFeatureTour::RegisterProfilePrefs(client().registry());
  controller().ToggleWidget();
  auto& feature_tour = controller().feature_tour_for_testing();
  views::test::WidgetVisibleWaiter(feature_tour.widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(),
            feature_tour.widget_for_testing()->GetNativeWindow());
  EXPECT_EQ(focus_controller->GetFocusedWindow(),
            feature_tour.widget_for_testing()->GetNativeWindow());

  // Complete the feature tour.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  views::test::WidgetDestroyedWaiter(feature_tour.widget_for_testing()).Wait();
  ASSERT_NE(controller().widget_for_testing(), nullptr);
  views::test::WidgetVisibleWaiter(controller().widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(),
            controller().widget_for_testing()->GetNativeWindow());
  EXPECT_EQ(focus_controller->GetFocusedWindow(),
            controller().widget_for_testing()->GetNativeWindow());
  auto* view = views::AsViewClass<QuickInsertView>(
      controller().widget_for_testing()->widget_delegate()->GetContentsView());
  ASSERT_NE(view, nullptr);
  EXPECT_TRUE(
      view->search_field_view_for_testing().textfield_for_testing().HasFocus());

  // Dismiss Quick Insert.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);
  views::test::WidgetDestroyedWaiter(controller().widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(), nullptr);
  EXPECT_EQ(focus_controller->GetFocusedWindow(), nullptr);
}

TEST_F(QuickInsertControllerTest,
       ToggleWidgetShowsWidgetAfterCompletingFeatureTourWithoutFocus) {
  std::unique_ptr<views::Widget> test_widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .SetShow(true)
          .BuildClientOwnsWidget();
  wm::FocusController* focus_controller = Shell::Get()->focus_controller();
  ASSERT_EQ(focus_controller->GetActiveWindow(),
            test_widget->GetNativeWindow());
  ASSERT_EQ(focus_controller->GetFocusedWindow(),
            test_widget->GetNativeWindow());

  // Show the feature tour.
  PickerFeatureTour::RegisterProfilePrefs(client().registry());
  controller().ToggleWidget();
  auto& feature_tour = controller().feature_tour_for_testing();
  views::test::WidgetVisibleWaiter(feature_tour.widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(),
            feature_tour.widget_for_testing()->GetNativeWindow());
  EXPECT_EQ(focus_controller->GetFocusedWindow(),
            feature_tour.widget_for_testing()->GetNativeWindow());

  // Complete the feature tour.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  views::test::WidgetDestroyedWaiter(feature_tour.widget_for_testing()).Wait();
  ASSERT_NE(controller().widget_for_testing(), nullptr);
  views::test::WidgetVisibleWaiter(controller().widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(),
            controller().widget_for_testing()->GetNativeWindow());
  EXPECT_EQ(focus_controller->GetFocusedWindow(),
            controller().widget_for_testing()->GetNativeWindow());
  auto* view = views::AsViewClass<QuickInsertView>(
      controller().widget_for_testing()->widget_delegate()->GetContentsView());
  ASSERT_NE(view, nullptr);
  EXPECT_TRUE(
      view->search_field_view_for_testing().textfield_for_testing().HasFocus());

  // Dismiss Quick Insert.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);
  views::test::WidgetDestroyedWaiter(controller().widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(),
            test_widget->GetNativeWindow());
  EXPECT_EQ(focus_controller->GetFocusedWindow(),
            test_widget->GetNativeWindow());
}

TEST_F(QuickInsertControllerTest,
       ToggleWidgetShowsWidgetAfterCompletingFeatureTourWithFocus) {
  std::unique_ptr<views::Widget> textfield_widget =
      ash::TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .SetShow(true)
          .BuildClientOwnsWidget();
  auto* textfield =
      textfield_widget->SetContentsView(std::make_unique<views::Textfield>());
  textfield->GetViewAccessibility().SetName(u"textfield");
  textfield->RequestFocus();
  wm::FocusController* focus_controller = Shell::Get()->focus_controller();
  ASSERT_EQ(focus_controller->GetActiveWindow(),
            textfield_widget->GetNativeWindow());
  ASSERT_EQ(focus_controller->GetFocusedWindow(),
            textfield_widget->GetNativeWindow());
  ASSERT_TRUE(textfield->HasFocus());

  // Show the feature tour.
  PickerFeatureTour::RegisterProfilePrefs(client().registry());
  controller().ToggleWidget();
  auto& feature_tour = controller().feature_tour_for_testing();
  views::test::WidgetVisibleWaiter(feature_tour.widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(),
            feature_tour.widget_for_testing()->GetNativeWindow());
  EXPECT_EQ(focus_controller->GetFocusedWindow(),
            feature_tour.widget_for_testing()->GetNativeWindow());
  EXPECT_FALSE(textfield->HasFocus());

  // Complete the feature tour.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  views::test::WidgetDestroyedWaiter(feature_tour.widget_for_testing()).Wait();
  ASSERT_NE(controller().widget_for_testing(), nullptr);
  views::test::WidgetVisibleWaiter(controller().widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(),
            controller().widget_for_testing()->GetNativeWindow());
  EXPECT_EQ(focus_controller->GetFocusedWindow(),
            controller().widget_for_testing()->GetNativeWindow());
  EXPECT_FALSE(textfield->HasFocus());
  auto* view = views::AsViewClass<QuickInsertView>(
      controller().widget_for_testing()->widget_delegate()->GetContentsView());
  ASSERT_NE(view, nullptr);
  EXPECT_TRUE(
      view->search_field_view_for_testing().textfield_for_testing().HasFocus());

  // Dismiss Quick Insert.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);
  views::test::WidgetDestroyedWaiter(controller().widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(),
            textfield_widget->GetNativeWindow());
  EXPECT_EQ(focus_controller->GetFocusedWindow(),
            textfield_widget->GetNativeWindow());
  EXPECT_TRUE(textfield->HasFocus());
}

TEST_F(QuickInsertControllerTest, ToggleWidgetOpensUrlAfterLearnMore) {
  PickerFeatureTour::RegisterProfilePrefs(client().registry());
  controller().ToggleWidget();
  auto& feature_tour = controller().feature_tour_for_testing();
  views::test::WidgetVisibleWaiter(feature_tour.widget_for_testing()).Wait();

  EXPECT_CALL(
      mock_new_window_delegate(),
      OpenUrl(Property("host", &GURL::host_piece, "support.google.com"), _, _))
      .Times(1);

  const views::Button* button = feature_tour.learn_more_button_for_testing();
  ASSERT_NE(button, nullptr);
  LeftClickOn(button);
  views::test::WidgetDestroyedWaiter(feature_tour.widget_for_testing()).Wait();

  EXPECT_FALSE(controller().widget_for_testing());
}

TEST_F(QuickInsertControllerTest, SetClientToNullKeepsWidget) {
  controller().ToggleWidget();

  controller().SetClient(nullptr);

  EXPECT_TRUE(controller().widget_for_testing());
}

TEST_F(QuickInsertControllerTest, ShowWidgetRecordsInputReadyLatency) {
  base::HistogramTester histogram;

  controller().ToggleWidget(base::TimeTicks::Now());
  views::test::WidgetVisibleWaiter widget_visible_waiter(
      controller().widget_for_testing());
  widget_visible_waiter.Wait();

  histogram.ExpectTotalCount("Ash.Picker.Session.InputReadyLatency", 1);
}

TEST_F(QuickInsertControllerTest, InsertResultDoesNothingWhenWidgetIsClosed) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertTextResult(u"abc"));
  ui::FakeTextInputClient input_field(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&input_field);
  absl::Cleanup focused_input_field_reset = [input_method] {
    // Reset the input field since it will be destroyed before `input_method`.
    input_method->SetFocusedTextInputClient(nullptr);
  };

  EXPECT_EQ(input_field.text(), u"");
}

TEST_F(QuickInsertControllerTest,
       InsertTextResultInsertsIntoInputFieldAfterFocus) {
  controller().ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertTextResult(u"abc"));
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
  ui::FakeTextInputClient input_field(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&input_field);
  absl::Cleanup focused_input_field_reset = [input_method] {
    // Reset the input field since it will be destroyed before `input_method`.
    input_method->SetFocusedTextInputClient(nullptr);
  };

  EXPECT_EQ(input_field.text(), u"abc");
}

TEST_F(QuickInsertControllerTest,
       InsertClipboardResultPastesIntoInputFieldAfterFocus) {
  controller().ToggleWidget();
  ASSERT_TRUE(CopyTextToClipboard());
  std::optional<base::UnguessableToken> clipboard_item_id =
      GetFirstClipboardItemId();
  ASSERT_TRUE(clipboard_item_id.has_value());

  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertClipboardResult(
          *clipboard_item_id, QuickInsertClipboardResult::DisplayFormat::kText,
          /*file_count=*/0,
          /*display_text=*/u"", /*display_image=*/{}, /*is_recent=*/false));
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
  ClipboardPasteWaiter waiter;
  // Create a new to focus on.
  auto new_widget = CreateFramelessTestWidget();

  waiter.Wait();
}

TEST_F(QuickInsertControllerTest,
       InsertGifResultInsertsIntoInputFieldAfterFocus) {
  controller().ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller().CloseWidgetThenInsertResultOnNextFocus(QuickInsertGifResult(
      GURL("http://foo.com/fake_preview.gif"),
      GURL("http://foo.com/fake_preview_image.png"), gfx::Size(),
      GURL("http://foo.com/fake.gif"), gfx::Size(),
      /*content_description=*/u""));
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
  ui::FakeTextInputClient input_field(
      input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.last_inserted_image_url(),
            GURL("http://foo.com/fake.gif"));
}

TEST_F(QuickInsertControllerTest,
       InsertUnsupportedImageResultTimeoutCopiesToClipboard) {
  controller().ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller().CloseWidgetThenInsertResultOnNextFocus(QuickInsertGifResult(
      /*preview_url=*/GURL("http://foo.com/preview"),
      /*preview_image_url=*/GURL(), gfx::Size(30, 20),
      /*full_url=*/GURL("http://foo.com"), gfx::Size(60, 40),
      /*content_description=*/u"a gif"));
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
  ui::FakeTextInputClient input_field(
      input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});
  input_method->SetFocusedTextInputClient(&input_field);
  task_environment()->FastForwardBy(QuickInsertController::kInsertMediaTimeout);

  EXPECT_EQ(
      ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
      uR"html(<img src="http://foo.com/" referrerpolicy="no-referrer" alt="a gif" width="60" height="40"/>)html");
  EXPECT_TRUE(
      ash::ToastManager::Get()->IsToastShown("picker_copy_to_clipboard"));
}

TEST_F(QuickInsertControllerTest,
       InsertBrowsingHistoryResultInsertsIntoInputFieldAfterFocus) {
  controller().ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertBrowsingHistoryResult(GURL("http://foo.com"), u"Foo",
                                       ui::ImageModel{}));
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"http://foo.com/");
}

TEST_F(QuickInsertControllerTest, InsertResultClosesWidgetImmediately) {
  controller().ToggleWidget();

  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertTextResult(u"abc"));

  EXPECT_TRUE(controller().widget_for_testing()->IsClosed());
}

TEST_F(QuickInsertControllerTest,
       InsertResultDelaysWidgetCloseForAccessibility) {
  controller().ToggleWidget();
  Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
      true, A11Y_NOTIFICATION_NONE);

  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertTextResult(u"abc"));

  EXPECT_FALSE(controller().widget_for_testing()->IsClosed());
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
}

TEST_F(QuickInsertControllerTest, OpenBrowsingHistoryResult) {
  controller().ToggleWidget();

  EXPECT_CALL(mock_new_window_delegate(), OpenUrl(GURL("http://foo.com"), _, _))
      .Times(1);

  controller().OpenResult(QuickInsertBrowsingHistoryResult(
      GURL("http://foo.com"), u"Foo", ui::ImageModel{}));
}

TEST_F(QuickInsertControllerTest, OpenDriveFileResult) {
  controller().ToggleWidget();

  EXPECT_CALL(mock_new_window_delegate(), OpenUrl(GURL("http://foo.com"), _, _))
      .Times(1);

  controller().OpenResult(QuickInsertDriveFileResult(
      /*id=*/std::nullopt, u"title", GURL("http://foo.com"), base::FilePath()));
}

TEST_F(QuickInsertControllerTest, OpenLocalFileResult) {
  controller().ToggleWidget();

  EXPECT_CALL(mock_new_window_delegate(), OpenFile(base::FilePath("abc.png")))
      .Times(1);

  controller().OpenResult(
      QuickInsertLocalFileResult(u"title", base::FilePath("abc.png")));
}

TEST_F(QuickInsertControllerTest, OpenNewGoogleDocOpensGoogleDocs) {
  controller().ToggleWidget();

  EXPECT_CALL(mock_new_window_delegate(),
              OpenUrl(GURL("https://docs.new"), _, _))
      .Times(1);

  controller().OpenResult(
      QuickInsertNewWindowResult(QuickInsertNewWindowResult::Type::kDoc));
}

TEST_F(QuickInsertControllerTest,
       OpenCapsLockResultTurnsOnCapsLockOnNextFocus) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  controller().ToggleWidget();

  controller().OpenResult(QuickInsertCapsLockResult(
      /*enabled=*/true, QuickInsertCapsLockResult::Shortcut::kAltSearch));
  input_method->SetFocusedTextInputClient(&input_field);

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_TRUE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(QuickInsertControllerTest,
       OpenCapsLockResultTurnsOffCapsLockOnNextFocus) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  controller().ToggleWidget();

  controller().OpenResult(QuickInsertCapsLockResult(
      /*enabled=*/false, QuickInsertCapsLockResult::Shortcut::kAltSearch));
  input_method->SetFocusedTextInputClient(&input_field);

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_FALSE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(QuickInsertControllerTest, OpenCapsLockResultTurnsOnCapsLockOnTimeout) {
  controller().ToggleWidget();

  controller().OpenResult(QuickInsertCapsLockResult(
      /*enabled=*/true, QuickInsertCapsLockResult::Shortcut::kAltSearch));
  task_environment()->FastForwardBy(base::Seconds(1));

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_TRUE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(QuickInsertControllerTest, OpenCapsLockResultTurnsOffCapsLockOnTimeout) {
  controller().ToggleWidget();

  controller().OpenResult(QuickInsertCapsLockResult(
      /*enabled=*/false, QuickInsertCapsLockResult::Shortcut::kAltSearch));
  task_environment()->FastForwardBy(base::Seconds(1));

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_FALSE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(QuickInsertControllerTest, OpenUpperCaseResultCommitsUpperCase) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"aBc DeF", gfx::Range(0, 7));

  controller().ToggleWidget();
  controller().OpenResult(QuickInsertCaseTransformResult(
      QuickInsertCaseTransformResult::Type::kUpperCase));
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"ABC DEF");
}

TEST_F(QuickInsertControllerTest, OpenLowerCaseResultCommitsLowerCase) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"aBc DeF", gfx::Range(0, 7));

  controller().ToggleWidget();
  controller().OpenResult(QuickInsertCaseTransformResult(
      QuickInsertCaseTransformResult::Type::kLowerCase));
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"abc def");
}

TEST_F(QuickInsertControllerTest, OpenTitleCaseResultCommitsTitleCase) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"aBc DeF", gfx::Range(0, 7));

  controller().ToggleWidget();
  controller().OpenResult(QuickInsertCaseTransformResult(
      QuickInsertCaseTransformResult::Type::kTitleCase));
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"Abc Def");
}

TEST_F(QuickInsertControllerTest, ShowEmojiPickerCallsEmojiPanelCallback) {
  controller().ToggleWidget();
  base::test::TestFuture<ui::EmojiPickerCategory, ui::EmojiPickerFocusBehavior,
                         const std::string&>
      future;
  ui::SetShowEmojiKeyboardCallback(future.GetRepeatingCallback());

  controller().ShowEmojiPicker(ui::EmojiPickerCategory::kSymbols, u"abc");

  const auto& [category, focus_behavior, initial_query] = future.Get();
  EXPECT_EQ(category, ui::EmojiPickerCategory::kSymbols);
  EXPECT_EQ(focus_behavior, ui::EmojiPickerFocusBehavior::kAlwaysShow);
  EXPECT_EQ(initial_query, "abc");
}

TEST_F(QuickInsertControllerTest, ShowingAndClosingWidgetRecordsUsageMetrics) {
  base::HistogramTester histogram_tester;

  // Show the widget twice.
  controller().ToggleWidget();
  task_environment()->FastForwardBy(base::Seconds(1));
  controller().widget_for_testing()->CloseNow();
  task_environment()->FastForwardBy(base::Seconds(2));
  controller().ToggleWidget();
  task_environment()->FastForwardBy(base::Seconds(3));
  controller().widget_for_testing()->CloseNow();
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

TEST_F(QuickInsertControllerTest, ShowEditorCallsCallbackFromClient) {
  base::test::TestFuture<std::optional<std::string>, std::optional<std::string>>
      show_editor_future;
  EXPECT_CALL(client(), CacheEditorContext)
      .WillOnce(Return(show_editor_future.GetCallback()));

  controller().ToggleWidget();
  controller().ShowEditor(/*preset_query_id=*/"preset",
                          /*freeform_text=*/"freeform");

  EXPECT_THAT(show_editor_future.Get(), FieldsAre("preset", "freeform"));
}

TEST_F(QuickInsertControllerTest, ShowLobsterCallsCallbackFromClient) {
  base::test::TestFuture<std::optional<std::string>> show_lobster_future;
  EXPECT_CALL(client(), CacheLobsterContext)
      .WillOnce(Return(show_lobster_future.GetCallback()));

  controller().ToggleWidget();
  controller().ShowLobster(/*freeform_text=*/"freeform");

  EXPECT_THAT(show_lobster_future.Get(), "freeform");
}

TEST_F(QuickInsertControllerTest,
       GetResultsForCategoryReturnsEmptyForEmptyResults) {
  base::test::TestFuture<std::vector<QuickInsertSearchResultsSection>> future;
  EXPECT_CALL(client(), GetSuggestedLinkResults)
      .WillRepeatedly(
          [](size_t max_results,
             TestQuickInsertClient::SuggestedLinksCallback callback) {
            std::move(callback).Run({});
          });

  controller().ToggleWidget();
  controller().GetResultsForCategory(QuickInsertCategory::kLinks,
                                     future.GetRepeatingCallback());

  EXPECT_THAT(future.Take(), IsEmpty());
}

TEST_F(QuickInsertControllerTest,
       AvailableCategoriesContainsEditorWhenEnabled) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.Focus();

  EXPECT_CALL(client(), CacheEditorContext).WillOnce(Return(base::DoNothing()));

  controller().ToggleWidget();

  EXPECT_THAT(controller().GetAvailableCategories(),
              Contains(QuickInsertCategory::kEditorWrite));
}

TEST_F(QuickInsertControllerTest,
       AvailableCategoriesDoesNotContainEditorWhenDisabled) {
  EXPECT_CALL(client(), CacheEditorContext)
      .WillOnce(Return(base::NullCallback()));

  controller().ToggleWidget();

  EXPECT_THAT(controller().GetAvailableCategories(),
              Not(Contains(QuickInsertCategory::kEditorWrite)));
}

TEST_F(QuickInsertControllerTest, SuggestedEmojiReturnsDefaultEmojisWhenEmpty) {
  controller().ToggleWidget();

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              ElementsAre(QuickInsertEmojiResult::Emoji(u"🙂"),
                          QuickInsertEmojiResult::Emoji(u"😂"),
                          QuickInsertEmojiResult::Emoji(u"🤔"),
                          QuickInsertEmojiResult::Emoji(u"😢"),
                          QuickInsertEmojiResult::Emoji(u"👏"),
                          QuickInsertEmojiResult::Emoji(u"👍")));
}

TEST_F(QuickInsertControllerTest,
       SuggestedEmojiReturnsRecentEmojiFollowedByDefaultEmojis) {
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(client().GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  controller().ToggleWidget();

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              ElementsAre(QuickInsertEmojiResult::Emoji(u"abc"),
                          QuickInsertEmojiResult::Emoji(u"xyz"),
                          QuickInsertEmojiResult::Emoji(u"🙂"),
                          QuickInsertEmojiResult::Emoji(u"😂"),
                          QuickInsertEmojiResult::Emoji(u"🤔"),
                          QuickInsertEmojiResult::Emoji(u"😢")));
}

TEST_F(QuickInsertControllerTest, AddsNewRecentEmoji) {
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(client().GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertEmojiResult::Emoji(u"def"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              ElementsAre(QuickInsertEmojiResult::Emoji(u"def"),
                          QuickInsertEmojiResult::Emoji(u"abc"),
                          QuickInsertEmojiResult::Emoji(u"xyz"),
                          QuickInsertEmojiResult::Emoji(u"🙂"),
                          QuickInsertEmojiResult::Emoji(u"😂"),
                          QuickInsertEmojiResult::Emoji(u"🤔")));
}

TEST_F(QuickInsertControllerTest, AddsExistingRecentEmoji) {
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(client().GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertEmojiResult::Emoji(u"xyz"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              ElementsAre(QuickInsertEmojiResult::Emoji(u"xyz"),
                          QuickInsertEmojiResult::Emoji(u"abc"),
                          QuickInsertEmojiResult::Emoji(u"🙂"),
                          QuickInsertEmojiResult::Emoji(u"😂"),
                          QuickInsertEmojiResult::Emoji(u"🤔"),
                          QuickInsertEmojiResult::Emoji(u"😢")));
}

TEST_F(QuickInsertControllerTest, AddsRecentEmojiEmptyHistory) {
  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertEmojiResult::Emoji(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              ElementsAre(QuickInsertEmojiResult::Emoji(u"abc"),
                          QuickInsertEmojiResult::Emoji(u"🙂"),
                          QuickInsertEmojiResult::Emoji(u"😂"),
                          QuickInsertEmojiResult::Emoji(u"🤔"),
                          QuickInsertEmojiResult::Emoji(u"😢"),
                          QuickInsertEmojiResult::Emoji(u"👏")));
}

TEST_F(QuickInsertControllerTest, RecentlyAddedEmojiHasCorrectType) {
  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertEmojiResult::Emoji(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              Contains(QuickInsertEmojiResult::Emoji(u"abc")));
}

TEST_F(QuickInsertControllerTest, RecentlyAddedSymbolHasCorrectType) {
  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertEmojiResult::Symbol(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              Contains(QuickInsertEmojiResult::Symbol(u"abc")));
}

TEST_F(QuickInsertControllerTest, RecentlyAddedEmoticonHasCorrectType) {
  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertEmojiResult::Emoticon(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              Contains(QuickInsertEmojiResult::Emoticon(u"abc")));
}

TEST_F(QuickInsertControllerTest, AddRecentEmojiWithFocus) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(
      input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .should_do_learning = true});
  input_method->SetFocusedTextInputClient(&input_field);

  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertEmojiResult::Emoji(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              Contains(QuickInsertEmojiResult::Emoji(u"abc")));
}

TEST_F(QuickInsertControllerTest, DoesNotAddRecentEmojiWithFocusIfIncognito) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(
      input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .should_do_learning = false});
  input_method->SetFocusedTextInputClient(&input_field);

  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      QuickInsertEmojiResult::Emoji(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              Not(Contains(QuickInsertEmojiResult::Emoji(u"abc"))));
}

TEST_F(QuickInsertControllerTest,
       SuggestedEmojiReturnsRecentEmojiEmoticonAndSymbol) {
  base::Value::List emoji_history_value;
  emoji_history_value.Append(
      base::Value::Dict().Set("text", "emoji1").Set("timestamp", "10"));
  emoji_history_value.Append(
      base::Value::Dict().Set("text", "emoji2").Set("timestamp", "5"));
  base::Value::List emoticon_history_value;
  emoticon_history_value.Append(
      base::Value::Dict().Set("text", "emoticon1").Set("timestamp", "12"));
  emoticon_history_value.Append(
      base::Value::Dict().Set("text", "emoticon2").Set("timestamp", "2"));
  base::Value::List symbol_history_value;
  symbol_history_value.Append(
      base::Value::Dict().Set("text", "symbol1").Set("timestamp", "15"));
  symbol_history_value.Append(
      base::Value::Dict().Set("text", "symbol2").Set("timestamp", "8"));
  ScopedDictPrefUpdate update(client().GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(emoji_history_value));
  update->Set("emoticon", std::move(emoticon_history_value));
  update->Set("symbol", std::move(symbol_history_value));

  controller().ToggleWidget();

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              ElementsAre(QuickInsertEmojiResult::Symbol(u"symbol1"),
                          QuickInsertEmojiResult::Emoticon(u"emoticon1"),
                          QuickInsertEmojiResult::Emoji(u"emoji1"),
                          QuickInsertEmojiResult::Symbol(u"symbol2"),
                          QuickInsertEmojiResult::Emoji(u"emoji2"),
                          QuickInsertEmojiResult::Emoticon(u"emoticon2")));
}

TEST_F(QuickInsertControllerTest, SearchesCapsLockOnWhenCapsLockIsOff) {
  base::test::TestFuture<std::vector<QuickInsertSearchResultsSection>>
      search_future;

  controller().ToggleWidget();
  controller().StartSearch(u"caps", /*category=*/{},
                           search_future.GetRepeatingCallback());

  EXPECT_THAT(search_future.Take(),
              Contains(Property(
                  &QuickInsertSearchResultsSection::results,
                  Contains(QuickInsertCapsLockResult(
                      /*enabled=*/true,
                      QuickInsertCapsLockResult::Shortcut::kAltLauncher)))));
}

TEST_F(QuickInsertControllerTest, SearchesCapsLockOffWhenCapsLockIsOn) {
  base::test::TestFuture<std::vector<QuickInsertSearchResultsSection>>
      search_future;
  GetImeKeyboard()->SetCapsLockEnabled(true);

  controller().ToggleWidget();
  controller().StartSearch(u"caps", /*category=*/{},
                           search_future.GetRepeatingCallback());

  EXPECT_THAT(search_future.Take(),
              Contains(Property(
                  &QuickInsertSearchResultsSection::results,
                  Contains(QuickInsertCapsLockResult(
                      /*enabled=*/false,
                      QuickInsertCapsLockResult::Shortcut::kAltLauncher)))));
}

TEST_F(QuickInsertControllerTest,
       DoesNotSearchCaseTransformWhenNoSelectedText) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  base::MockCallback<QuickInsertController::SearchResultsCallback> callback;

  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(Contains(Property(
                  &QuickInsertSearchResultsSection::results,
                  Contains(VariantWith<QuickInsertCaseTransformResult>(_))))))
      .Times(0);

  controller().ToggleWidget();
  controller().StartSearch(u"uppercase", /*category=*/{}, callback.Get());
}

TEST_F(QuickInsertControllerTest, SearchesCaseTransformWhenSelectedText) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method->SetFocusedTextInputClient(&input_field);
  base::MockCallback<QuickInsertController::SearchResultsCallback> callback;

  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(Contains(Property(
                  &QuickInsertSearchResultsSection::results,
                  Contains(VariantWith<QuickInsertCaseTransformResult>(
                      Field(&QuickInsertCaseTransformResult::type,
                            QuickInsertCaseTransformResult::kUpperCase)))))))
      .Times(1);

  controller().ToggleWidget();
  controller().StartSearch(u"uppercase", /*category=*/{}, callback.Get());
}

TEST_F(QuickInsertControllerTest, IsValidDuringWidgetClose) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  controller().ToggleWidget();
  views::test::WidgetVisibleWaiter(controller().widget_for_testing()).Wait();

  controller().ToggleWidget();
  controller().GetActionForResult(QuickInsertTextResult(u"a"));
  controller().IsGifsEnabled();
  controller().GetAvailableCategories();
}

TEST_F(QuickInsertControllerTest,
       ReturnsCapsLockPositionTopWhenCapsLockHasNotShownEnoughTimes) {
  prefs().SetInteger(prefs::kPickerCapsLockDislayedCountPrefName, 4);
  prefs().SetInteger(prefs::kPickerCapsLockSelectedCountPrefName, 0);
  EXPECT_EQ(controller().GetCapsLockPosition(), PickerCapsLockPosition::kTop);
}

TEST_F(QuickInsertControllerTest,
       ReturnsCapsLockPositionTopWhenCapsLockIsAlwaysUsed) {
  prefs().SetInteger(prefs::kPickerCapsLockDislayedCountPrefName, 15);
  prefs().SetInteger(prefs::kPickerCapsLockSelectedCountPrefName, 14);
  EXPECT_EQ(controller().GetCapsLockPosition(), PickerCapsLockPosition::kTop);
}

TEST_F(QuickInsertControllerTest,
       ReturnsCapsLockPositionMiddleWhenCapsLockIsSometimesUsed) {
  prefs().SetInteger(prefs::kPickerCapsLockDislayedCountPrefName, 15);
  prefs().SetInteger(prefs::kPickerCapsLockSelectedCountPrefName, 7);
  EXPECT_EQ(controller().GetCapsLockPosition(),
            PickerCapsLockPosition::kMiddle);
}

TEST_F(QuickInsertControllerTest,
       ReturnsCapsLockPositionBottomWhenCapsLockIsNeverUsed) {
  prefs().SetInteger(prefs::kPickerCapsLockDislayedCountPrefName, 15);
  prefs().SetInteger(prefs::kPickerCapsLockSelectedCountPrefName, 0);
  EXPECT_EQ(controller().GetCapsLockPosition(),
            PickerCapsLockPosition::kBottom);
}

TEST_F(QuickInsertControllerTest,
       ReturnCapsLockPositionTopWhenCapsLockIsEnabled) {
  prefs().SetInteger(prefs::kPickerCapsLockDislayedCountPrefName, 4);
  prefs().SetInteger(prefs::kPickerCapsLockSelectedCountPrefName, 0);
  GetImeKeyboard()->SetCapsLockEnabled(true);

  EXPECT_EQ(controller().GetCapsLockPosition(), PickerCapsLockPosition::kTop);
}

struct ActionTestCase {
  QuickInsertSearchResult result;
  std::optional<QuickInsertActionType> unfocused_action;
  std::optional<QuickInsertActionType> no_selection_action;
  std::optional<QuickInsertActionType> has_selection_action;
};

class QuickInsertControllerActionTest
    : public QuickInsertControllerTest,
      public testing::WithParamInterface<ActionTestCase> {};

TEST_P(QuickInsertControllerActionTest, GetActionForResultUnfocused) {
  controller().ToggleWidget();

  if (GetParam().unfocused_action.has_value()) {
    EXPECT_EQ(controller().GetActionForResult(GetParam().result),
              GetParam().unfocused_action);
  }
}

TEST_P(QuickInsertControllerActionTest, GetActionForResultNoSelection) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  controller().ToggleWidget();

  if (GetParam().no_selection_action.has_value()) {
    EXPECT_EQ(controller().GetActionForResult(GetParam().result),
              GetParam().no_selection_action);
  }
}

TEST_P(QuickInsertControllerActionTest, GetActionForResultHasSelection) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  controller().ToggleWidget();

  if (GetParam().has_selection_action.has_value()) {
    EXPECT_EQ(controller().GetActionForResult(GetParam().result),
              GetParam().has_selection_action);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    QuickInsertControllerActionTest,
    testing::ValuesIn<ActionTestCase>({
        {
            .result = QuickInsertTextResult(u""),
            .no_selection_action = QuickInsertActionType::kInsert,
            .has_selection_action = QuickInsertActionType::kInsert,
        },
        {
            .result = QuickInsertEmojiResult::Emoji(u""),
            .no_selection_action = QuickInsertActionType::kInsert,
            .has_selection_action = QuickInsertActionType::kInsert,
        },
        {
            .result = QuickInsertEmojiResult::Symbol(u""),
            .no_selection_action = QuickInsertActionType::kInsert,
            .has_selection_action = QuickInsertActionType::kInsert,
        },
        {
            .result = QuickInsertEmojiResult::Emoticon(u""),
            .no_selection_action = QuickInsertActionType::kInsert,
            .has_selection_action = QuickInsertActionType::kInsert,
        },
        {
            .result = QuickInsertClipboardResult(
                base::UnguessableToken::Create(),
                QuickInsertClipboardResult::DisplayFormat::kFile,
                0,
                u"",
                {},
                false),
            .no_selection_action = QuickInsertActionType::kInsert,
            .has_selection_action = QuickInsertActionType::kInsert,
        },
        {
            .result = QuickInsertGifResult({}, {}, {}, {}, {}, u""),
            .no_selection_action = QuickInsertActionType::kInsert,
            .has_selection_action = QuickInsertActionType::kInsert,
        },
        {
            .result = QuickInsertBrowsingHistoryResult({}, u"", {}),
            .unfocused_action = QuickInsertActionType::kOpen,
            .no_selection_action = QuickInsertActionType::kInsert,
            .has_selection_action = QuickInsertActionType::kInsert,
        },
        {
            .result = QuickInsertLocalFileResult(u"", {}),
            .unfocused_action = QuickInsertActionType::kOpen,
            .no_selection_action = QuickInsertActionType::kInsert,
            .has_selection_action = QuickInsertActionType::kInsert,
        },
        {
            .result = QuickInsertDriveFileResult(std::nullopt, u"", {}, {}),
            .unfocused_action = QuickInsertActionType::kOpen,
            .no_selection_action = QuickInsertActionType::kInsert,
            .has_selection_action = QuickInsertActionType::kInsert,
        },
        {
            .result =
                QuickInsertCategoryResult(QuickInsertCategory::kEmojisGifs),
            .unfocused_action = QuickInsertActionType::kDo,
            .no_selection_action = QuickInsertActionType::kDo,
            .has_selection_action = QuickInsertActionType::kDo,
        },
        {
            .result = QuickInsertCategoryResult(QuickInsertCategory::kEmojis),
            .unfocused_action = QuickInsertActionType::kDo,
            .no_selection_action = QuickInsertActionType::kDo,
            .has_selection_action = QuickInsertActionType::kDo,
        },
        {
            .result = QuickInsertSearchRequestResult(u"", u"", {}),
            .unfocused_action = QuickInsertActionType::kDo,
            .no_selection_action = QuickInsertActionType::kDo,
            .has_selection_action = QuickInsertActionType::kDo,
        },
        {
            .result =
                QuickInsertEditorResult(QuickInsertEditorResult::Mode::kWrite,
                                        u"",
                                        {},
                                        {}),
            .unfocused_action = QuickInsertActionType::kCreate,
            .no_selection_action = QuickInsertActionType::kCreate,
            .has_selection_action = QuickInsertActionType::kCreate,
        },
        {
            .result = QuickInsertNewWindowResult(
                QuickInsertNewWindowResult::Type::kDoc),
            .unfocused_action = QuickInsertActionType::kDo,
        },
    }));

}  // namespace
}  // namespace ash
