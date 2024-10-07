// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

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
#include "ash/picker/metrics/picker_session_metrics.h"
#include "ash/picker/mock_picker_client.h"
#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/model/picker_caps_lock_position.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_feature_tour.h"
#include "ash/picker/views/picker_search_bar_textfield.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_view.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
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

// A PickerClient implementation used for testing.
// Automatically sets itself as the client when it's created, and unsets itself
// when it's destroyed.
class TestPickerClient : public MockPickerClient {
 public:
  TestPickerClient(PickerController* controller,
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
  ~TestPickerClient() override { controller_->SetClient(nullptr); }

  PrefRegistrySimple* registry() { return prefs_->registry(); }

 private:
  raw_ptr<PickerController> controller_ = nullptr;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_ = nullptr;
};

class PickerControllerTest : public AshTestBase {
 public:
  PickerControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<PickerController>();
    client_ = std::make_unique<NiceMock<TestPickerClient>>(controller_.get(),
                                                           &prefs_);
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

  PickerController& controller() { return *controller_; }

  NiceMock<TestPickerClient>& client() { return *client_; }

  sync_preferences::TestingPrefServiceSyncable& prefs() { return prefs_; }

  metrics::structured::TestStructuredMetricsRecorder& metrics_recorder() {
    return *metrics_recorder_;
  }

 private:
  MockNewWindowDelegate new_window_delegate_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<PickerController> controller_;
  std::unique_ptr<NiceMock<TestPickerClient>> client_;
  std::unique_ptr<metrics::structured::TestStructuredMetricsRecorder>
      metrics_recorder_;
};

TEST_F(PickerControllerTest, ToggleWidgetShowsWidgetIfClosed) {
  controller().ToggleWidget();

  EXPECT_TRUE(controller().widget_for_testing());
}

TEST_F(PickerControllerTest,
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

TEST_F(PickerControllerTest, TogglingWidgetRecordsStartSessionMetrics) {
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

TEST_F(PickerControllerTest, ToggleWidgetClosesWidgetIfOpen) {
  controller().ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());

  controller().ToggleWidget();

  widget_destroyed_waiter.Wait();
  EXPECT_FALSE(controller().widget_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetShowsWidgetIfOpenedThenClosed) {
  controller().ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
  controller().ToggleWidget();
  widget_destroyed_waiter.Wait();

  controller().ToggleWidget();

  EXPECT_TRUE(controller().widget_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetShowsFeatureTourForFirstTime) {
  PickerFeatureTour::RegisterProfilePrefs(client().registry());
  controller().ToggleWidget();

  EXPECT_TRUE(controller().feature_tour_for_testing().widget_for_testing());
  EXPECT_FALSE(controller().widget_for_testing());
}

TEST_F(PickerControllerTest,
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
  auto* view = views::AsViewClass<PickerView>(
      controller().widget_for_testing()->widget_delegate()->GetContentsView());
  ASSERT_NE(view, nullptr);
  EXPECT_TRUE(
      view->search_field_view_for_testing().textfield_for_testing().HasFocus());

  // Dismiss Picker.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);
  views::test::WidgetDestroyedWaiter(controller().widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(), nullptr);
  EXPECT_EQ(focus_controller->GetFocusedWindow(), nullptr);
}

TEST_F(PickerControllerTest,
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
  auto* view = views::AsViewClass<PickerView>(
      controller().widget_for_testing()->widget_delegate()->GetContentsView());
  ASSERT_NE(view, nullptr);
  EXPECT_TRUE(
      view->search_field_view_for_testing().textfield_for_testing().HasFocus());

  // Dismiss Picker.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);
  views::test::WidgetDestroyedWaiter(controller().widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(),
            test_widget->GetNativeWindow());
  EXPECT_EQ(focus_controller->GetFocusedWindow(),
            test_widget->GetNativeWindow());
}

TEST_F(PickerControllerTest,
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
  auto* view = views::AsViewClass<PickerView>(
      controller().widget_for_testing()->widget_delegate()->GetContentsView());
  ASSERT_NE(view, nullptr);
  EXPECT_TRUE(
      view->search_field_view_for_testing().textfield_for_testing().HasFocus());

  // Dismiss Picker.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);
  views::test::WidgetDestroyedWaiter(controller().widget_for_testing()).Wait();
  EXPECT_EQ(focus_controller->GetActiveWindow(),
            textfield_widget->GetNativeWindow());
  EXPECT_EQ(focus_controller->GetFocusedWindow(),
            textfield_widget->GetNativeWindow());
  EXPECT_TRUE(textfield->HasFocus());
}

TEST_F(PickerControllerTest, ToggleWidgetOpensUrlAfterLearnMore) {
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

TEST_F(PickerControllerTest, SetClientToNullKeepsWidget) {
  controller().ToggleWidget();

  controller().SetClient(nullptr);

  EXPECT_TRUE(controller().widget_for_testing());
}

TEST_F(PickerControllerTest, ShowWidgetRecordsInputReadyLatency) {
  base::HistogramTester histogram;

  controller().ToggleWidget(base::TimeTicks::Now());
  views::test::WidgetVisibleWaiter widget_visible_waiter(
      controller().widget_for_testing());
  widget_visible_waiter.Wait();

  histogram.ExpectTotalCount("Ash.Picker.Session.InputReadyLatency", 1);
}

TEST_F(PickerControllerTest, InsertResultDoesNothingWhenWidgetIsClosed) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller().CloseWidgetThenInsertResultOnNextFocus(PickerTextResult(u"abc"));
  ui::FakeTextInputClient input_field(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&input_field);
  absl::Cleanup focused_input_field_reset = [input_method] {
    // Reset the input field since it will be destroyed before `input_method`.
    input_method->SetFocusedTextInputClient(nullptr);
  };

  EXPECT_EQ(input_field.text(), u"");
}

TEST_F(PickerControllerTest, InsertTextResultInsertsIntoInputFieldAfterFocus) {
  controller().ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller().CloseWidgetThenInsertResultOnNextFocus(PickerTextResult(u"abc"));
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

TEST_F(PickerControllerTest,
       InsertClipboardResultPastesIntoInputFieldAfterFocus) {
  controller().ToggleWidget();
  ASSERT_TRUE(CopyTextToClipboard());
  std::optional<base::UnguessableToken> clipboard_item_id =
      GetFirstClipboardItemId();
  ASSERT_TRUE(clipboard_item_id.has_value());

  controller().CloseWidgetThenInsertResultOnNextFocus(PickerClipboardResult(
      *clipboard_item_id, PickerClipboardResult::DisplayFormat::kText,
      /*file_count=*/0,
      /*display_text=*/u"", /*display_image=*/{}, /*is_recent=*/false));
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
  ClipboardPasteWaiter waiter;
  // Create a new to focus on.
  auto new_widget = CreateFramelessTestWidget();

  waiter.Wait();
}

TEST_F(PickerControllerTest, InsertGifResultInsertsIntoInputFieldAfterFocus) {
  controller().ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller().CloseWidgetThenInsertResultOnNextFocus(
      PickerGifResult(GURL("http://foo.com/fake_preview.gif"),
                      GURL("http://foo.com/fake_preview_image.png"),
                      gfx::Size(), GURL("http://foo.com/fake.gif"), gfx::Size(),
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

TEST_F(PickerControllerTest,
       InsertUnsupportedImageResultTimeoutCopiesToClipboard) {
  controller().ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller().CloseWidgetThenInsertResultOnNextFocus(PickerGifResult(
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
  task_environment()->FastForwardBy(PickerController::kInsertMediaTimeout);

  EXPECT_EQ(
      ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
      uR"html(<img src="http://foo.com/" referrerpolicy="no-referrer" alt="a gif" width="60" height="40"/>)html");
  EXPECT_TRUE(
      ash::ToastManager::Get()->IsToastShown("picker_copy_to_clipboard"));
}

TEST_F(PickerControllerTest,
       InsertBrowsingHistoryResultInsertsIntoInputFieldAfterFocus) {
  controller().ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller().CloseWidgetThenInsertResultOnNextFocus(
      PickerBrowsingHistoryResult(GURL("http://foo.com"), u"Foo",
                                  ui::ImageModel{}));
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"http://foo.com/");
}

TEST_F(PickerControllerTest, InsertResultClosesWidgetImmediately) {
  controller().ToggleWidget();

  controller().CloseWidgetThenInsertResultOnNextFocus(PickerTextResult(u"abc"));

  EXPECT_TRUE(controller().widget_for_testing()->IsClosed());
}

TEST_F(PickerControllerTest, InsertResultDelaysWidgetCloseForAccessibility) {
  controller().ToggleWidget();
  Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
      true, A11Y_NOTIFICATION_NONE);

  controller().CloseWidgetThenInsertResultOnNextFocus(PickerTextResult(u"abc"));

  EXPECT_FALSE(controller().widget_for_testing()->IsClosed());
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller().widget_for_testing());
}

TEST_F(PickerControllerTest, OpenBrowsingHistoryResult) {
  controller().ToggleWidget();

  EXPECT_CALL(mock_new_window_delegate(), OpenUrl(GURL("http://foo.com"), _, _))
      .Times(1);

  controller().OpenResult(PickerBrowsingHistoryResult(
      GURL("http://foo.com"), u"Foo", ui::ImageModel{}));
}

TEST_F(PickerControllerTest, OpenDriveFileResult) {
  controller().ToggleWidget();

  EXPECT_CALL(mock_new_window_delegate(), OpenUrl(GURL("http://foo.com"), _, _))
      .Times(1);

  controller().OpenResult(PickerDriveFileResult(
      /*id=*/std::nullopt, u"title", GURL("http://foo.com"), base::FilePath()));
}

TEST_F(PickerControllerTest, OpenLocalFileResult) {
  controller().ToggleWidget();

  EXPECT_CALL(mock_new_window_delegate(), OpenFile(base::FilePath("abc.png")))
      .Times(1);

  controller().OpenResult(
      PickerLocalFileResult(u"title", base::FilePath("abc.png")));
}

TEST_F(PickerControllerTest, OpenNewGoogleDocOpensGoogleDocs) {
  controller().ToggleWidget();

  EXPECT_CALL(mock_new_window_delegate(),
              OpenUrl(GURL("https://docs.new"), _, _))
      .Times(1);

  controller().OpenResult(
      PickerNewWindowResult(PickerNewWindowResult::Type::kDoc));
}

TEST_F(PickerControllerTest, OpenCapsLockResultTurnsOnCapsLockOnNextFocus) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  controller().ToggleWidget();

  controller().OpenResult(PickerCapsLockResult(
      /*enabled=*/true, PickerCapsLockResult::Shortcut::kAltSearch));
  input_method->SetFocusedTextInputClient(&input_field);

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_TRUE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(PickerControllerTest, OpenCapsLockResultTurnsOffCapsLockOnNextFocus) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  controller().ToggleWidget();

  controller().OpenResult(PickerCapsLockResult(
      /*enabled=*/false, PickerCapsLockResult::Shortcut::kAltSearch));
  input_method->SetFocusedTextInputClient(&input_field);

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_FALSE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(PickerControllerTest, OpenCapsLockResultTurnsOnCapsLockOnTimeout) {
  controller().ToggleWidget();

  controller().OpenResult(PickerCapsLockResult(
      /*enabled=*/true, PickerCapsLockResult::Shortcut::kAltSearch));
  task_environment()->FastForwardBy(base::Seconds(1));

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_TRUE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(PickerControllerTest, OpenCapsLockResultTurnsOffCapsLockOnTimeout) {
  controller().ToggleWidget();

  controller().OpenResult(PickerCapsLockResult(
      /*enabled=*/false, PickerCapsLockResult::Shortcut::kAltSearch));
  task_environment()->FastForwardBy(base::Seconds(1));

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_FALSE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(PickerControllerTest, OpenUpperCaseResultCommitsUpperCase) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"aBc DeF", gfx::Range(0, 7));

  controller().ToggleWidget();
  controller().OpenResult(
      PickerCaseTransformResult(PickerCaseTransformResult::Type::kUpperCase));
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"ABC DEF");
}

TEST_F(PickerControllerTest, OpenLowerCaseResultCommitsLowerCase) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"aBc DeF", gfx::Range(0, 7));

  controller().ToggleWidget();
  controller().OpenResult(
      PickerCaseTransformResult(PickerCaseTransformResult::Type::kLowerCase));
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"abc def");
}

TEST_F(PickerControllerTest, OpenTitleCaseResultCommitsTitleCase) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"aBc DeF", gfx::Range(0, 7));

  controller().ToggleWidget();
  controller().OpenResult(
      PickerCaseTransformResult(PickerCaseTransformResult::Type::kTitleCase));
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"Abc Def");
}

TEST_F(PickerControllerTest, ShowEmojiPickerCallsEmojiPanelCallback) {
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

TEST_F(PickerControllerTest, ShowingAndClosingWidgetRecordsUsageMetrics) {
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

TEST_F(PickerControllerTest, ShowEditorCallsCallbackFromClient) {
  base::test::TestFuture<std::optional<std::string>, std::optional<std::string>>
      show_editor_future;
  EXPECT_CALL(client(), CacheEditorContext)
      .WillOnce(Return(show_editor_future.GetCallback()));

  controller().ToggleWidget();
  controller().ShowEditor(/*preset_query_id=*/"preset",
                          /*freeform_text=*/"freeform");

  EXPECT_THAT(show_editor_future.Get(), FieldsAre("preset", "freeform"));
}

TEST_F(PickerControllerTest, ShowLobsterCallsCallbackFromClient) {
  base::test::TestFuture<std::optional<std::string>> show_lobster_future;
  EXPECT_CALL(client(), GetShowLobsterCallback)
      .WillOnce(Return(show_lobster_future.GetCallback()));

  controller().ToggleWidget();
  controller().ShowLobster(/*freeform_text=*/"freeform");

  EXPECT_THAT(show_lobster_future.Get(), "freeform");
}

TEST_F(PickerControllerTest, GetResultsForCategoryReturnsEmptyForEmptyResults) {
  base::test::TestFuture<std::vector<PickerSearchResultsSection>> future;
  EXPECT_CALL(client(), GetSuggestedLinkResults)
      .WillRepeatedly([](size_t max_results,
                         TestPickerClient::SuggestedLinksCallback callback) {
        std::move(callback).Run({});
      });

  controller().ToggleWidget();
  controller().GetResultsForCategory(PickerCategory::kLinks,
                                     future.GetRepeatingCallback());

  EXPECT_THAT(future.Take(), IsEmpty());
}

TEST_F(PickerControllerTest, AvailableCategoriesContainsEditorWhenEnabled) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.Focus();

  EXPECT_CALL(client(), CacheEditorContext).WillOnce(Return(base::DoNothing()));

  controller().ToggleWidget();

  EXPECT_THAT(controller().GetAvailableCategories(),
              Contains(PickerCategory::kEditorWrite));
}

TEST_F(PickerControllerTest,
       AvailableCategoriesDoesNotContainEditorWhenDisabled) {
  EXPECT_CALL(client(), CacheEditorContext)
      .WillOnce(Return(base::NullCallback()));

  controller().ToggleWidget();

  EXPECT_THAT(controller().GetAvailableCategories(),
              Not(Contains(PickerCategory::kEditorWrite)));
}

TEST_F(PickerControllerTest, SuggestedEmojiReturnsDefaultEmojisWhenEmpty) {
  controller().ToggleWidget();

  EXPECT_THAT(
      controller().GetSuggestedEmoji(),
      ElementsAre(
          PickerEmojiResult::Emoji(u"🙂"), PickerEmojiResult::Emoji(u"😂"),
          PickerEmojiResult::Emoji(u"🤔"), PickerEmojiResult::Emoji(u"😢"),
          PickerEmojiResult::Emoji(u"👏"), PickerEmojiResult::Emoji(u"👍")));
}

TEST_F(PickerControllerTest,
       SuggestedEmojiReturnsRecentEmojiFollowedByDefaultEmojis) {
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(client().GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  controller().ToggleWidget();

  EXPECT_THAT(
      controller().GetSuggestedEmoji(),
      ElementsAre(
          PickerEmojiResult::Emoji(u"abc"), PickerEmojiResult::Emoji(u"xyz"),
          PickerEmojiResult::Emoji(u"🙂"), PickerEmojiResult::Emoji(u"😂"),
          PickerEmojiResult::Emoji(u"🤔"), PickerEmojiResult::Emoji(u"😢")));
}

TEST_F(PickerControllerTest, AddsNewRecentEmoji) {
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(client().GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      PickerEmojiResult::Emoji(u"def"));

  EXPECT_THAT(
      controller().GetSuggestedEmoji(),
      ElementsAre(
          PickerEmojiResult::Emoji(u"def"), PickerEmojiResult::Emoji(u"abc"),
          PickerEmojiResult::Emoji(u"xyz"), PickerEmojiResult::Emoji(u"🙂"),
          PickerEmojiResult::Emoji(u"😂"), PickerEmojiResult::Emoji(u"🤔")));
}

TEST_F(PickerControllerTest, AddsExistingRecentEmoji) {
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(client().GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      PickerEmojiResult::Emoji(u"xyz"));

  EXPECT_THAT(
      controller().GetSuggestedEmoji(),
      ElementsAre(
          PickerEmojiResult::Emoji(u"xyz"), PickerEmojiResult::Emoji(u"abc"),
          PickerEmojiResult::Emoji(u"🙂"), PickerEmojiResult::Emoji(u"😂"),
          PickerEmojiResult::Emoji(u"🤔"), PickerEmojiResult::Emoji(u"😢")));
}

TEST_F(PickerControllerTest, AddsRecentEmojiEmptyHistory) {
  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      PickerEmojiResult::Emoji(u"abc"));

  EXPECT_THAT(
      controller().GetSuggestedEmoji(),
      ElementsAre(
          PickerEmojiResult::Emoji(u"abc"), PickerEmojiResult::Emoji(u"🙂"),
          PickerEmojiResult::Emoji(u"😂"), PickerEmojiResult::Emoji(u"🤔"),
          PickerEmojiResult::Emoji(u"😢"), PickerEmojiResult::Emoji(u"👏")));
}

TEST_F(PickerControllerTest, RecentlyAddedEmojiHasCorrectType) {
  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      PickerEmojiResult::Emoji(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              Contains(PickerEmojiResult::Emoji(u"abc")));
}

TEST_F(PickerControllerTest, RecentlyAddedSymbolHasCorrectType) {
  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      PickerEmojiResult::Symbol(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              Contains(PickerEmojiResult::Symbol(u"abc")));
}

TEST_F(PickerControllerTest, RecentlyAddedEmoticonHasCorrectType) {
  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      PickerEmojiResult::Emoticon(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              Contains(PickerEmojiResult::Emoticon(u"abc")));
}

TEST_F(PickerControllerTest, AddRecentEmojiWithFocus) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(
      input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .should_do_learning = true});
  input_method->SetFocusedTextInputClient(&input_field);

  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      PickerEmojiResult::Emoji(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              Contains(PickerEmojiResult::Emoji(u"abc")));
}

TEST_F(PickerControllerTest, DoesNotAddRecentEmojiWithFocusIfIncognito) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(
      input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .should_do_learning = false});
  input_method->SetFocusedTextInputClient(&input_field);

  controller().ToggleWidget();
  controller().CloseWidgetThenInsertResultOnNextFocus(
      PickerEmojiResult::Emoji(u"abc"));

  EXPECT_THAT(controller().GetSuggestedEmoji(),
              Not(Contains(PickerEmojiResult::Emoji(u"abc"))));
}

TEST_F(PickerControllerTest,
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
              ElementsAre(PickerEmojiResult::Symbol(u"symbol1"),
                          PickerEmojiResult::Emoticon(u"emoticon1"),
                          PickerEmojiResult::Emoji(u"emoji1"),
                          PickerEmojiResult::Symbol(u"symbol2"),
                          PickerEmojiResult::Emoji(u"emoji2"),
                          PickerEmojiResult::Emoticon(u"emoticon2")));
}

TEST_F(PickerControllerTest, SearchesCapsLockOnWhenCapsLockIsOff) {
  base::test::TestFuture<std::vector<PickerSearchResultsSection>> search_future;

  controller().ToggleWidget();
  controller().StartSearch(u"caps", /*category=*/{},
                           search_future.GetRepeatingCallback());

  EXPECT_THAT(
      search_future.Take(),
      Contains(Property(&PickerSearchResultsSection::results,
                        Contains(PickerCapsLockResult(
                            /*enabled=*/true,
                            PickerCapsLockResult::Shortcut::kAltLauncher)))));
}

TEST_F(PickerControllerTest, SearchesCapsLockOffWhenCapsLockIsOn) {
  base::test::TestFuture<std::vector<PickerSearchResultsSection>> search_future;
  GetImeKeyboard()->SetCapsLockEnabled(true);

  controller().ToggleWidget();
  controller().StartSearch(u"caps", /*category=*/{},
                           search_future.GetRepeatingCallback());

  EXPECT_THAT(
      search_future.Take(),
      Contains(Property(&PickerSearchResultsSection::results,
                        Contains(PickerCapsLockResult(
                            /*enabled=*/false,
                            PickerCapsLockResult::Shortcut::kAltLauncher)))));
}

TEST_F(PickerControllerTest, DoesNotSearchCaseTransformWhenNoSelectedText) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  base::MockCallback<PickerController::SearchResultsCallback> callback;

  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(Contains(Property(
                  &PickerSearchResultsSection::results,
                  Contains(VariantWith<PickerCaseTransformResult>(_))))))
      .Times(0);

  controller().ToggleWidget();
  controller().StartSearch(u"uppercase", /*category=*/{}, callback.Get());
}

TEST_F(PickerControllerTest, SearchesCaseTransformWhenSelectedText) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method->SetFocusedTextInputClient(&input_field);
  base::MockCallback<PickerController::SearchResultsCallback> callback;

  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(Contains(Property(
                  &PickerSearchResultsSection::results,
                  Contains(VariantWith<PickerCaseTransformResult>(
                      Field(&PickerCaseTransformResult::type,
                            PickerCaseTransformResult::kUpperCase)))))))
      .Times(1);

  controller().ToggleWidget();
  controller().StartSearch(u"uppercase", /*category=*/{}, callback.Get());
}

TEST_F(PickerControllerTest, IsValidDuringWidgetClose) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  controller().ToggleWidget();
  views::test::WidgetVisibleWaiter(controller().widget_for_testing()).Wait();

  controller().ToggleWidget();
  controller().GetActionForResult(PickerTextResult(u"a"));
  controller().IsGifsEnabled();
  controller().GetAvailableCategories();
}

TEST_F(PickerControllerTest,
       ReturnsCapsLockPositionTopWhenCapsLockHasNotShownEnoughTimes) {
  prefs().SetInteger(prefs::kPickerCapsLockDislayedCountPrefName, 4);
  prefs().SetInteger(prefs::kPickerCapsLockSelectedCountPrefName, 0);
  EXPECT_EQ(controller().GetCapsLockPosition(), PickerCapsLockPosition::kTop);
}

TEST_F(PickerControllerTest,
       ReturnsCapsLockPositionTopWhenCapsLockIsAlwaysUsed) {
  prefs().SetInteger(prefs::kPickerCapsLockDislayedCountPrefName, 15);
  prefs().SetInteger(prefs::kPickerCapsLockSelectedCountPrefName, 14);
  EXPECT_EQ(controller().GetCapsLockPosition(), PickerCapsLockPosition::kTop);
}

TEST_F(PickerControllerTest,
       ReturnsCapsLockPositionMiddleWhenCapsLockIsSometimesUsed) {
  prefs().SetInteger(prefs::kPickerCapsLockDislayedCountPrefName, 15);
  prefs().SetInteger(prefs::kPickerCapsLockSelectedCountPrefName, 7);
  EXPECT_EQ(controller().GetCapsLockPosition(),
            PickerCapsLockPosition::kMiddle);
}

TEST_F(PickerControllerTest,
       ReturnsCapsLockPositionBottomWhenCapsLockIsNeverUsed) {
  prefs().SetInteger(prefs::kPickerCapsLockDislayedCountPrefName, 15);
  prefs().SetInteger(prefs::kPickerCapsLockSelectedCountPrefName, 0);
  EXPECT_EQ(controller().GetCapsLockPosition(),
            PickerCapsLockPosition::kBottom);
}

struct ActionTestCase {
  PickerSearchResult result;
  std::optional<PickerActionType> unfocused_action;
  std::optional<PickerActionType> no_selection_action;
  std::optional<PickerActionType> has_selection_action;
};

class PickerControllerActionTest
    : public PickerControllerTest,
      public testing::WithParamInterface<ActionTestCase> {};

TEST_P(PickerControllerActionTest, GetActionForResultUnfocused) {
  controller().ToggleWidget();

  if (GetParam().unfocused_action.has_value()) {
    EXPECT_EQ(controller().GetActionForResult(GetParam().result),
              GetParam().unfocused_action);
  }
}

TEST_P(PickerControllerActionTest, GetActionForResultNoSelection) {
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

TEST_P(PickerControllerActionTest, GetActionForResultHasSelection) {
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
    PickerControllerActionTest,
    testing::ValuesIn<ActionTestCase>({
        {
            .result = PickerTextResult(u""),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerEmojiResult::Emoji(u""),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerEmojiResult::Symbol(u""),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerEmojiResult::Emoticon(u""),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerClipboardResult(
                base::UnguessableToken::Create(),
                PickerClipboardResult::DisplayFormat::kFile,
                0,
                u"",
                {},
                false),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerGifResult({}, {}, {}, {}, {}, u""),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerBrowsingHistoryResult({}, u"", {}),
            .unfocused_action = PickerActionType::kOpen,
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerLocalFileResult(u"", {}),
            .unfocused_action = PickerActionType::kOpen,
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerDriveFileResult(std::nullopt, u"", {}, {}),
            .unfocused_action = PickerActionType::kOpen,
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerCategoryResult(PickerCategory::kEmojisGifs),
            .unfocused_action = PickerActionType::kDo,
            .no_selection_action = PickerActionType::kDo,
            .has_selection_action = PickerActionType::kDo,
        },
        {
            .result = PickerCategoryResult(PickerCategory::kEmojis),
            .unfocused_action = PickerActionType::kDo,
            .no_selection_action = PickerActionType::kDo,
            .has_selection_action = PickerActionType::kDo,
        },
        {
            .result = PickerSearchRequestResult(u"", u"", {}),
            .unfocused_action = PickerActionType::kDo,
            .no_selection_action = PickerActionType::kDo,
            .has_selection_action = PickerActionType::kDo,
        },
        {
            .result = PickerEditorResult(PickerEditorResult::Mode::kWrite,
                                         u"",
                                         {},
                                         {}),
            .unfocused_action = PickerActionType::kCreate,
            .no_selection_action = PickerActionType::kCreate,
            .has_selection_action = PickerActionType::kCreate,
        },
        {
            .result = PickerNewWindowResult(PickerNewWindowResult::Type::kDoc),
            .unfocused_action = PickerActionType::kDo,
        },
    }));

}  // namespace
}  // namespace ash
