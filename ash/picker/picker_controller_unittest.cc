// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include <string>

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_test_util.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/picker/mock_picker_client.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
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
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;
using ::testing::VariantWith;

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

class PickerControllerTest : public AshTestBase {
 public:
  PickerControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(std::move(delegate));
  }

  MockNewWindowDelegate& mock_new_window_delegate() {
    return *new_window_delegate_;
  }

 private:
  std::unique_ptr<TestNewWindowDelegateProvider> delegate_provider_;
  // Holds a raw ptr to the `MockNewWindowDelegate` owned by
  // `delegate_provider_`.
  raw_ptr<MockNewWindowDelegate> new_window_delegate_;
};

// A PickerClient implementation used for testing.
// Automatically sets itself as the client when it's created, and unsets itself
// when it's destroyed.
class TestPickerClient : public MockPickerClient {
 public:
  explicit TestPickerClient(PickerController* controller)
      : controller_(controller) {
    controller_->SetClient(this);
    prefs_.registry()->RegisterDictionaryPref(prefs::kEmojiPickerHistory);
    // Set default behaviours. These can be overridden with `WillOnce` and
    // `WillRepeatedly`.
    ON_CALL(*this, IsFeatureAllowedForDogfood).WillByDefault(Return(true));
    ON_CALL(*this, GetPrefs).WillByDefault(Return(&prefs_));
  }
  ~TestPickerClient() override { controller_->SetClient(nullptr); }

  PrefRegistrySimple* registry() { return prefs_.registry(); }

 private:
  raw_ptr<PickerController> controller_ = nullptr;
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(PickerControllerTest, ToggleWidgetShowsWidgetIfClosed) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  controller.ToggleWidget();

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest,
       ToggleWidgetInPasswordFieldTogglesCapslockAndShowsBubbleForAShortTime) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_PASSWORD});
  input_method->SetFocusedTextInputClient(&input_field);

  controller.ToggleWidget();
  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);

  EXPECT_FALSE(controller.widget_for_testing());
  EXPECT_TRUE(controller.caps_lock_state_view_for_testing());
  EXPECT_TRUE(ime_keyboard->IsCapsLockEnabled());

  task_environment()->FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(controller.caps_lock_state_view_for_testing());
}

TEST_F(PickerControllerTest,
       ToggleWidgetTwiceQuicklyInPasswordFieldExtendsBubbleShowTime) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_PASSWORD});
  input_method->SetFocusedTextInputClient(&input_field);

  controller.ToggleWidget();
  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);

  EXPECT_FALSE(controller.widget_for_testing());
  EXPECT_TRUE(controller.caps_lock_state_view_for_testing());
  EXPECT_TRUE(ime_keyboard->IsCapsLockEnabled());

  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(controller.caps_lock_state_view_for_testing());

  controller.ToggleWidget();

  EXPECT_FALSE(controller.widget_for_testing());
  EXPECT_TRUE(controller.caps_lock_state_view_for_testing());
  EXPECT_FALSE(ime_keyboard->IsCapsLockEnabled());

  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(controller.caps_lock_state_view_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetClosesWidgetIfOpen) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  controller.ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller.widget_for_testing());

  controller.ToggleWidget();

  widget_destroyed_waiter.Wait();
  EXPECT_FALSE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetShowsWidgetIfOpenedThenClosed) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  controller.ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller.widget_for_testing());
  controller.ToggleWidget();
  widget_destroyed_waiter.Wait();

  controller.ToggleWidget();

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetShowsFeatureTourForFirstTime) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  RegisterUserProfilePrefs(client.registry(), /*country=*/"",
                           /*for_test=*/true);
  controller.ToggleWidget();

  EXPECT_TRUE(controller.feature_tour_for_testing().widget_for_testing());
  EXPECT_FALSE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest,
       ToggleWidgetShowsWidgetAfterCompletingFeatureTour) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  RegisterUserProfilePrefs(client.registry(), /*country=*/"",
                           /*for_test=*/true);
  controller.ToggleWidget();
  auto& feature_tour = controller.feature_tour_for_testing();
  views::test::WidgetVisibleWaiter(feature_tour.widget_for_testing()).Wait();
  views::Button* button = feature_tour.complete_button_for_testing();
  ASSERT_NE(button, nullptr);
  ViewDrawnWaiter().Wait(button);
  LeftClickOn(button);
  views::test::WidgetDestroyedWaiter(feature_tour.widget_for_testing()).Wait();

  views::test::WidgetVisibleWaiter(controller.widget_for_testing()).Wait();
}

TEST_F(PickerControllerTest,
       ToggleWidgetShowsWidgetForDogfoodWhenClientAllowed) {
  base::test::ScopedFeatureList features(ash::features::kPickerDogfood);
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  EXPECT_CALL(client, IsFeatureAllowedForDogfood).WillOnce(Return(true));

  controller.ToggleWidget();

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest,
       ToggleWidgetDoesNotShowWidgetWhenClientDisallowsDogfood) {
  base::test::ScopedFeatureList features(ash::features::kPickerDogfood);
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  EXPECT_CALL(client, IsFeatureAllowedForDogfood).WillOnce(Return(false));

  controller.ToggleWidget();

  EXPECT_FALSE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, SetClientToNullKeepsWidget) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  controller.ToggleWidget();

  controller.SetClient(nullptr);

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ShowWidgetRecordsInputReadyLatency) {
  base::HistogramTester histogram;
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  controller.ToggleWidget(base::TimeTicks::Now());
  views::test::WidgetVisibleWaiter widget_visible_waiter(
      controller.widget_for_testing());
  widget_visible_waiter.Wait();

  histogram.ExpectTotalCount("Ash.Picker.Session.InputReadyLatency", 1);
}

TEST_F(PickerControllerTest, InsertResultDoesNothingWhenWidgetIsClosed) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
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
  NiceMock<TestPickerClient> client(&controller);
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
  NiceMock<TestPickerClient> client(&controller);
  controller.ToggleWidget();
  ASSERT_TRUE(CopyTextToClipboard());
  std::optional<base::UnguessableToken> clipboard_item_id =
      GetFirstClipboardItemId();
  ASSERT_TRUE(clipboard_item_id.has_value());

  controller.InsertResultOnNextFocus(PickerSearchResult::Clipboard(
      *clipboard_item_id,
      PickerSearchResult::ClipboardData::DisplayFormat::kText,
      /*display_text=*/u"", /*display_image=*/{}, /*is_recent=*/false));
  controller.widget_for_testing()->CloseNow();
  ClipboardPasteWaiter waiter;
  // Create a new to focus on.
  auto new_widget = CreateFramelessTestWidget();

  waiter.Wait();
}

TEST_F(PickerControllerTest,
       InsertBrowsingHistoryResultInsertsIntoInputFieldAfterFocus) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
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

TEST_F(PickerControllerTest, OpenBrowsingHistoryResult) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  EXPECT_CALL(mock_new_window_delegate(), OpenUrl(GURL("http://foo.com"), _, _))
      .Times(1);

  controller.OpenResult(PickerSearchResult::BrowsingHistory(
      GURL("http://foo.com"), u"Foo", ui::ImageModel{}));
}

TEST_F(PickerControllerTest, OpenDriveFileResult) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  EXPECT_CALL(mock_new_window_delegate(), OpenUrl(GURL("http://foo.com"), _, _))
      .Times(1);

  controller.OpenResult(PickerSearchResult::DriveFile(
      u"title", GURL("http://foo.com"), base::FilePath()));
}

TEST_F(PickerControllerTest, OpenLocalFileResult) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  EXPECT_CALL(mock_new_window_delegate(), OpenFile(base::FilePath("abc.png")))
      .Times(1);

  controller.OpenResult(
      PickerSearchResult::LocalFile(u"title", base::FilePath("abc.png")));
}

TEST_F(PickerControllerTest, OpenNewGoogleDocOpensGoogleDocs) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  EXPECT_CALL(mock_new_window_delegate(),
              OpenUrl(GURL("https://docs.new"), _, _))
      .Times(1);

  controller.OpenResult(PickerSearchResult::NewWindow(
      PickerSearchResult::NewWindowData::Type::kDoc));
}

TEST_F(PickerControllerTest, OpenCapsLockResultTurnsOnCapsLock) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  controller.ToggleWidget();

  controller.OpenResult(PickerSearchResult::CapsLock(true));

  input_method::ImeKeyboard* ime_keyboard = GetImeKeyboard();
  ASSERT_TRUE(ime_keyboard);
  EXPECT_TRUE(ime_keyboard->IsCapsLockEnabled());
}

TEST_F(PickerControllerTest, OpenCapsLockResultTurnsOffCapsLock) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  controller.ToggleWidget();

  controller.OpenResult(PickerSearchResult::CapsLock(false));

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
  input_field.SetTextAndSelection(u"abc", gfx::Range(0, 3));
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  controller.ToggleWidget();
  controller.OpenResult(PickerSearchResult::CaseTransform(
      PickerSearchResult::CaseTransformData::Type::kUpperCase));
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"ABC");
}

TEST_F(PickerControllerTest, OpenLowerCaseResultCommitsLowerCase) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"XYZ", gfx::Range(0, 3));
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  controller.ToggleWidget();
  controller.OpenResult(PickerSearchResult::CaseTransform(
      PickerSearchResult::CaseTransformData::Type::kLowerCase));
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"xyz");
}

TEST_F(PickerControllerTest, OpenTitleCaseResultCommitsTitleCase) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"how are you", gfx::Range(0, 11));
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  controller.ToggleWidget();
  controller.OpenResult(PickerSearchResult::CaseTransform(
      PickerSearchResult::CaseTransformData::Type::kTitleCase));
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"How Are You");
}

TEST_F(PickerControllerTest, OpenSentenceCaseResultCommitsSentenceCase) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  input_field.SetTextAndSelection(u"how are you? fine. thanks!  ok",
                                  gfx::Range(0, 30));
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  controller.ToggleWidget();
  controller.OpenResult(PickerSearchResult::CaseTransform(
      PickerSearchResult::CaseTransformData::Type::kSentenceCase));
  input_method->SetFocusedTextInputClient(&input_field);

  EXPECT_EQ(input_field.text(), u"How are you? Fine. Thanks!  Ok");
}
TEST_F(PickerControllerTest, ShowEmojiPickerCallsEmojiPanelCallback) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  controller.ToggleWidget();
  base::test::TestFuture<ui::EmojiPickerCategory, ui::EmojiPickerFocusBehavior,
                         const std::string&>
      future;
  ui::SetShowEmojiKeyboardCallback(future.GetRepeatingCallback());

  controller.ShowEmojiPicker(ui::EmojiPickerCategory::kSymbols, u"abc");

  const auto& [category, focus_behavior, initial_query] = future.Get();
  EXPECT_EQ(category, ui::EmojiPickerCategory::kSymbols);
  EXPECT_EQ(focus_behavior, ui::EmojiPickerFocusBehavior::kAlwaysShow);
  EXPECT_EQ(initial_query, "abc");
}

TEST_F(PickerControllerTest, ShowingAndClosingWidgetRecordsUsageMetrics) {
  base::HistogramTester histogram_tester;
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

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

TEST_F(PickerControllerTest, ShowEditorCallsCallbackFromClient) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  base::test::TestFuture<std::optional<std::string>, std::optional<std::string>>
      show_editor_future;
  EXPECT_CALL(client, CacheEditorContext)
      .WillOnce(Return(show_editor_future.GetCallback()));

  controller.ToggleWidget();
  controller.ShowEditor(/*preset_query_id=*/"preset",
                        /*freeform_text=*/"freeform");

  EXPECT_THAT(show_editor_future.Get(), FieldsAre("preset", "freeform"));
}

TEST_F(PickerControllerTest, GetResultsForCategoryReturnsEmptyForEmptyResults) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  base::test::TestFuture<std::vector<PickerSearchResultsSection>> future;
  EXPECT_CALL(client, GetSuggestedLinkResults)
      .WillRepeatedly([](TestPickerClient::SuggestedLinksCallback callback) {
        std::move(callback).Run({});
      });

  controller.ToggleWidget();
  controller.GetResultsForCategory(PickerCategory::kLinks,
                                   future.GetRepeatingCallback());

  EXPECT_THAT(future.Take(), IsEmpty());
}

TEST_F(PickerControllerTest, AvailableCategoriesContainsEditorWhenEnabled) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.Focus();
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  EXPECT_CALL(client, CacheEditorContext).WillOnce(Return(base::DoNothing()));

  controller.ToggleWidget();

  EXPECT_THAT(controller.GetAvailableCategories(),
              Contains(PickerCategory::kEditorWrite));
}

TEST_F(PickerControllerTest,
       AvailableCategoriesDoesNotContainEditorWhenDisabled) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  EXPECT_CALL(client, CacheEditorContext)
      .WillOnce(Return(base::NullCallback()));

  controller.ToggleWidget();

  EXPECT_THAT(controller.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEditorWrite)));
}

TEST_F(PickerControllerTest, SuggestedEmojiReturnsDefaultEmojisWhenEmpty) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  controller.ToggleWidget();

  EXPECT_THAT(
      controller.GetSuggestedEmoji(),
      ElementsAre(
          PickerSearchResult::Emoji(u"🙂"), PickerSearchResult::Emoji(u"😂"),
          PickerSearchResult::Emoji(u"🤔"), PickerSearchResult::Emoji(u"😢"),
          PickerSearchResult::Emoji(u"👏"), PickerSearchResult::Emoji(u"👍")));
}

TEST_F(PickerControllerTest,
       SuggestedEmojiReturnsRecentEmojiFollowedByDefaultEmojis) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(client.GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  controller.ToggleWidget();

  EXPECT_THAT(
      controller.GetSuggestedEmoji(),
      ElementsAre(
          PickerSearchResult::Emoji(u"abc"), PickerSearchResult::Emoji(u"xyz"),
          PickerSearchResult::Emoji(u"🙂"), PickerSearchResult::Emoji(u"😂"),
          PickerSearchResult::Emoji(u"🤔"), PickerSearchResult::Emoji(u"😢")));
}

TEST_F(PickerControllerTest, AddsNewRecentEmoji) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(client.GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  controller.ToggleWidget();
  controller.InsertResultOnNextFocus(PickerSearchResult::Emoji(u"def"));

  EXPECT_THAT(
      controller.GetSuggestedEmoji(),
      ElementsAre(
          PickerSearchResult::Emoji(u"def"), PickerSearchResult::Emoji(u"abc"),
          PickerSearchResult::Emoji(u"xyz"), PickerSearchResult::Emoji(u"🙂"),
          PickerSearchResult::Emoji(u"😂"), PickerSearchResult::Emoji(u"🤔")));
}

TEST_F(PickerControllerTest, AddsExistingRecentEmoji) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  base::Value::List history_value;
  history_value.Append(base::Value::Dict().Set("text", "abc"));
  history_value.Append(base::Value::Dict().Set("text", "xyz"));
  ScopedDictPrefUpdate update(client.GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(history_value));

  controller.ToggleWidget();
  controller.InsertResultOnNextFocus(PickerSearchResult::Emoji(u"xyz"));

  EXPECT_THAT(
      controller.GetSuggestedEmoji(),
      ElementsAre(
          PickerSearchResult::Emoji(u"xyz"), PickerSearchResult::Emoji(u"abc"),
          PickerSearchResult::Emoji(u"🙂"), PickerSearchResult::Emoji(u"😂"),
          PickerSearchResult::Emoji(u"🤔"), PickerSearchResult::Emoji(u"😢")));
}

TEST_F(PickerControllerTest, AddsRecentEmojiEmptyHistory) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  controller.ToggleWidget();
  controller.InsertResultOnNextFocus(PickerSearchResult::Emoji(u"abc"));

  EXPECT_THAT(
      controller.GetSuggestedEmoji(),
      ElementsAre(
          PickerSearchResult::Emoji(u"abc"), PickerSearchResult::Emoji(u"🙂"),
          PickerSearchResult::Emoji(u"😂"), PickerSearchResult::Emoji(u"🤔"),
          PickerSearchResult::Emoji(u"😢"), PickerSearchResult::Emoji(u"👏")));
}

TEST_F(PickerControllerTest,
       SuggestedEmojiReturnsRecentEmojiEmoticonAndSymbol) {
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
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
  ScopedDictPrefUpdate update(client.GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set("emoji", std::move(emoji_history_value));
  update->Set("emoticon", std::move(emoticon_history_value));
  update->Set("symbol", std::move(symbol_history_value));

  controller.ToggleWidget();

  EXPECT_THAT(controller.GetSuggestedEmoji(),
              ElementsAre(PickerSearchResult::Symbol(u"symbol1"),
                          PickerSearchResult::Emoticon(u"emoticon1"),
                          PickerSearchResult::Emoji(u"emoji1"),
                          PickerSearchResult::Symbol(u"symbol2"),
                          PickerSearchResult::Emoji(u"emoji2"),
                          PickerSearchResult::Emoticon(u"emoticon2")));
}

TEST_F(PickerControllerTest, SearchesCapsLockOnWhenCapsLockIsOff) {
  base::test::TestFuture<std::vector<PickerSearchResultsSection>> search_future;
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);

  controller.ToggleWidget();
  controller.StartSearch(u"caps", /*category=*/{},
                         search_future.GetRepeatingCallback());

  EXPECT_THAT(search_future.Take(),
              Contains(Property(&PickerSearchResultsSection::results,
                                Contains(PickerSearchResult::CapsLock(true)))));
}

TEST_F(PickerControllerTest, SearchesCapsLockOffWhenCapsLockIsOn) {
  base::test::TestFuture<std::vector<PickerSearchResultsSection>> search_future;
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  GetImeKeyboard()->SetCapsLockEnabled(true);

  controller.ToggleWidget();
  controller.StartSearch(u"caps", /*category=*/{},
                         search_future.GetRepeatingCallback());

  EXPECT_THAT(
      search_future.Take(),
      Contains(Property(&PickerSearchResultsSection::results,
                        Contains(PickerSearchResult::CapsLock(false)))));
}

TEST_F(PickerControllerTest, DoesNotSearchCaseTransformWhenNoSelectedText) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  base::MockCallback<PickerController::SearchResultsCallback> callback;

  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(
      callback,
      Run(Contains(Property(
          &PickerSearchResultsSection::results,
          Contains(Property(
              &PickerSearchResult::data,
              VariantWith<PickerSearchResult::CaseTransformData>(_)))))))
      .Times(0);

  controller.ToggleWidget();
  controller.StartSearch(u"uppercase", /*category=*/{}, callback.Get());
}

TEST_F(PickerControllerTest, SearchesCaseTransformWhenSelectedText) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method->SetFocusedTextInputClient(&input_field);
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  base::MockCallback<PickerController::SearchResultsCallback> callback;

  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(
      callback,
      Run(Contains(Property(
          &PickerSearchResultsSection::results,
          Contains(Property(
              &PickerSearchResult::data,
              VariantWith<PickerSearchResult::CaseTransformData>(Field(
                  &PickerSearchResult::CaseTransformData::type,
                  PickerSearchResult::CaseTransformData::kUpperCase))))))))
      .Times(1);

  controller.ToggleWidget();
  controller.StartSearch(u"uppercase", /*category=*/{}, callback.Get());
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
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  controller.ToggleWidget();

  if (GetParam().unfocused_action.has_value()) {
    EXPECT_EQ(controller.GetActionForResult(GetParam().result),
              GetParam().unfocused_action);
  }
}

TEST_P(PickerControllerActionTest, GetActionForResultNoSelection) {
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();
  ui::FakeTextInputClient input_field(input_method,
                                      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method->SetFocusedTextInputClient(&input_field);
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  controller.ToggleWidget();

  if (GetParam().no_selection_action.has_value()) {
    EXPECT_EQ(controller.GetActionForResult(GetParam().result),
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
  PickerController controller;
  NiceMock<TestPickerClient> client(&controller);
  controller.ToggleWidget();

  if (GetParam().has_selection_action.has_value()) {
    EXPECT_EQ(controller.GetActionForResult(GetParam().result),
              GetParam().has_selection_action);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerControllerActionTest,
    testing::ValuesIn<ActionTestCase>({
        {
            .result = PickerSearchResult::Text(u""),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerSearchResult::Emoji(u""),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerSearchResult::Symbol(u""),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerSearchResult::Emoticon(u""),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerSearchResult::Clipboard(
                base::UnguessableToken::Create(),
                PickerSearchResult::ClipboardData::DisplayFormat::kFile,
                u"",
                {},
                false),
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerSearchResult::BrowsingHistory({}, u"", {}),
            .unfocused_action = PickerActionType::kOpen,
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerSearchResult::LocalFile(u"", {}),
            .unfocused_action = PickerActionType::kOpen,
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result = PickerSearchResult::DriveFile(u"", {}, {}),
            .unfocused_action = PickerActionType::kOpen,
            .no_selection_action = PickerActionType::kInsert,
            .has_selection_action = PickerActionType::kInsert,
        },
        {
            .result =
                PickerSearchResult::Category(PickerCategory::kExpressions),
            .unfocused_action = PickerActionType::kDo,
            .no_selection_action = PickerActionType::kDo,
            .has_selection_action = PickerActionType::kDo,
        },
        {
            .result = PickerSearchResult::SearchRequest(u"", {}),
            .unfocused_action = PickerActionType::kDo,
            .no_selection_action = PickerActionType::kDo,
            .has_selection_action = PickerActionType::kDo,
        },
        {
            .result = PickerSearchResult::Editor(
                PickerSearchResult::EditorData::Mode::kWrite,
                u"",
                {},
                {}),
            .unfocused_action = PickerActionType::kCreate,
            .no_selection_action = PickerActionType::kCreate,
            .has_selection_action = PickerActionType::kCreate,
        },
        {
            .result = PickerSearchResult::NewWindow(
                PickerSearchResult::NewWindowData::Type::kDoc),
            .unfocused_action = PickerActionType::kDo,
        },
    }));

}  // namespace
}  // namespace ash
