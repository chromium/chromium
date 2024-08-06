// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "ash/system/mahi/test/mock_mahi_ui_controller_delegate.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/window_util.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/mahi/mahi_test_util.h"
#include "chrome/browser/ash/mahi/mahi_ui_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Aliases ---------------------------------------------------------------------

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithParamInterface;

// ViewDeletionObserver --------------------------------------------------------

// Runs a specific callback when the observed view is deleted.
class ViewDeletionObserver : public ::views::ViewObserver {
 public:
  ViewDeletionObserver(views::View* view,
                       base::RepeatingClosure on_delete_callback)
      : on_delete_callback_(on_delete_callback) {
    observation_.Observe(view);
  }

 private:
  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    observation_.Reset();
    on_delete_callback_.Run();
  }

  base::RepeatingClosure on_delete_callback_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

// UiUpdateRecorder ----------------------------------------------------------

// Records the types of the Mahi UI updates received during its life cycle.
class UiUpdateRecorder {
 public:
  explicit UiUpdateRecorder(MahiUiController* controller) {
    mock_controller_delegate_ =
        std::make_unique<NiceMock<MockMahiUiControllerDelegate>>(controller);
    ON_CALL(*mock_controller_delegate_, OnUpdated)
        .WillByDefault([this](const MahiUiUpdate& update) {
          received_updates_.insert(update.type());
        });
  }

  bool HasUpdate(MahiUiUpdateType type) const {
    return base::Contains(received_updates_, type);
  }

 private:
  std::unique_ptr<NiceMock<MockMahiUiControllerDelegate>>
      mock_controller_delegate_;

  std::set<MahiUiUpdateType> received_updates_;
};

// Helpers ---------------------------------------------------------------------

// Waits until the Mahi menu specified by `menu_view_widget` is closed.
void WaitUntilMahiMenuClosed(views::Widget* menu_view_widget) {
  ASSERT_TRUE(menu_view_widget);
  ASSERT_EQ(menu_view_widget->GetName(),
            chromeos::mahi::MahiMenuView::GetWidgetName());

  base::RunLoop run_loop;
  ViewDeletionObserver view_observer(
      menu_view_widget->GetContentsView(),
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
}

}  // namespace

// MahiUiBrowserTest -----------------------------------------------------------

// Tests Mahi UI features when opt-in flow is approved.
class MahiUiBrowserTest : public MahiUiBrowserTestBase {
 private:
  // MahiUiBrowserTestBase:
  void SetUpOnMainThread() override {
    MahiUiBrowserTestBase::SetUpOnMainThread();

    // Approve the Mahi feature to bypass opt-in flow.
    ApplyHMRConsentStatusAndWait(chromeos::HMRConsentStatus::kApproved);
  }
};

IN_PROC_BROWSER_TEST_F(MahiUiBrowserTest, MahiMenuZOrder) {
  EXPECT_FALSE(FindWidgetWithName(MahiPanelWidget::GetName()));

  // Have both the mahi menu and mahi panel open.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();
  views::Widget* mahi_menu_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::mahi::MahiMenuView::GetWidgetName());
  const views::View* const summary_button =
      mahi_menu_widget->GetContentsView()->GetViewByID(
          chromeos::mahi::ViewID::kSummaryButton);
  ASSERT_TRUE(summary_button);
  event_generator().MoveMouseTo(
      summary_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();
  mahi_menu_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::mahi::MahiMenuView::GetWidgetName());
  auto* mahi_panel_widget =
      FindWidgetWithNameAndWaitIfNeeded(MahiPanelWidget::GetName());
  ASSERT_TRUE(mahi_menu_widget);
  ASSERT_TRUE(mahi_panel_widget);

  // Expect the mahi menu widget to be in the top-most window compared to the
  // mahi panel widget.
  EXPECT_EQ(window_util::GetTopMostWindow(
                {mahi_menu_widget->GetNativeWindow()->parent(),
                 mahi_panel_widget->GetNativeWindow()->parent()}),
            mahi_menu_widget->GetNativeWindow()->parent());
}

IN_PROC_BROWSER_TEST_F(MahiUiBrowserTest, OnContextMenuClickedSettings) {
  // Ensure the Settings app installed.
  WaitForTestSystemAppInstall();

  // Open the Mahi menu by mouse right click on the web contents.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();
  views::Widget* const mahi_menu_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::mahi::MahiMenuView::GetWidgetName());
  ASSERT_TRUE(mahi_menu_widget);

  const views::View* const settings_button =
      mahi_menu_widget->GetContentsView()->GetViewByID(
          chromeos::mahi::ViewID::kSettingsButton);
  ASSERT_TRUE(settings_button);

  // Mouse click `settings_button` of the Mahi menu.
  event_generator().MoveMouseTo(
      settings_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  WaitForSettingsToLoad();

  // Verify that the Settings page is opened in a new window.
  const Browser* const settings_browser =
      chrome::SettingsWindowManager::GetInstance()->FindBrowserForProfile(
          browser()->profile());
  ASSERT_TRUE(settings_browser);
  EXPECT_NE(browser(), settings_browser);
  EXPECT_EQ(
      GURL(chrome::GetOSSettingsUrl(std::string())),
      settings_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(MahiUiBrowserTest, OnContextMenuClickedSummary) {
  EXPECT_FALSE(FindWidgetWithName(MahiPanelWidget::GetName()));

  // Open the Mahi menu by mouse right click on the web contents.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();
  views::Widget* const mahi_menu_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::mahi::MahiMenuView::GetWidgetName());
  ASSERT_TRUE(mahi_menu_widget);

  ui::ScopedAnimationDurationScaleMode zero_duration(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Open the Mahi panel by left clicking the menu's summary button.
  const views::View* const summary_button =
      mahi_menu_widget->GetContentsView()->GetViewByID(
          chromeos::mahi::ViewID::kSummaryButton);
  ASSERT_TRUE(summary_button);
  event_generator().MoveMouseTo(
      summary_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  // The summary could be loaded before the Mahi menu is closed. Therefore,
  // record Mahi UI updates during waiting.
  UiUpdateRecorder update_recorder(GetMahiUiController());

  WaitUntilMahiMenuClosed(mahi_menu_widget);

  // Check the existence of the Mahi panel.
  views::Widget* panel_widget =
      FindWidgetWithNameAndWaitIfNeeded(MahiPanelWidget::GetName());
  ASSERT_TRUE(panel_widget);

  // Wait until summary is loaded, if needed.
  if (!update_recorder.HasUpdate(MahiUiUpdateType::kSummaryLoaded)) {
    WaitUntilUiUpdateReceived(MahiUiUpdateType::kSummaryLoaded);
  }

  // The clipboard data should be empty before copying the summary.
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ASSERT_TRUE(clipboard);
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  ASSERT_FALSE(clipboard->GetClipboardData(&data_dst));

  const auto* const summary_label = views::AsViewClass<views::View>(
      panel_widget->GetContentsView()->GetViewByID(
          mahi_constants::ViewId::kSummaryLabel));
  ASSERT_TRUE(summary_label);

  panel_widget->LayoutRootViewIfNecessary();
  const gfx::Rect label_screen_bounds = summary_label->GetBoundsInScreen();

  // Select text of `summary_label` by mouse. Then copy the selected text.
  event_generator().MoveMouseTo(label_screen_bounds.left_center());
  event_generator().PressLeftButton();
  event_generator().MoveMouseTo(label_screen_bounds.right_center());
  event_generator().ReleaseLeftButton();
  event_generator().PressAndReleaseKeyAndModifierKeys(ui::VKEY_C,
                                                      ui::EF_CONTROL_DOWN);

  // Verify that the clipboard data is `summary_text`.
  const ui::ClipboardData* data = clipboard->GetClipboardData(&data_dst);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->text(), GetMahiDefaultTestSummary());
}

IN_PROC_BROWSER_TEST_F(MahiUiBrowserTest, OnContextMenuQuestionSent) {
  EXPECT_FALSE(FindWidgetWithName(MahiPanelWidget::GetName()));

  // Open the Mahi menu by mouse right click on the web contents.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();
  views::Widget* const mahi_menu_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::mahi::MahiMenuView::GetWidgetName());
  ASSERT_TRUE(mahi_menu_widget);

  const std::u16string question_text(u"question");
  TypeStringToMahiMenuTextfield(mahi_menu_widget, question_text);

  ui::ScopedAnimationDurationScaleMode zero_duration(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  const views::View* question_submit_button =
      mahi_menu_widget->GetContentsView()->GetViewByID(
          chromeos::mahi::ViewID::kSubmitQuestionButton);
  ASSERT_TRUE(question_submit_button);

  // Mouse click on `question_submit_button`.
  event_generator().MoveMouseTo(
      question_submit_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  // The answer could be loaded before the Mahi menu is closed. Therefore,
  // record Mahi UI updates during waiting.
  UiUpdateRecorder update_recorder(GetMahiUiController());

  WaitUntilMahiMenuClosed(mahi_menu_widget);

  // Check the existence of the Mahi panel.
  views::Widget* panel_widget =
      FindWidgetWithNameAndWaitIfNeeded(MahiPanelWidget::GetName());
  ASSERT_TRUE(panel_widget);

  // Wait until answer is loaded, if needed.
  if (!update_recorder.HasUpdate(MahiUiUpdateType::kAnswerLoaded)) {
    WaitUntilUiUpdateReceived(MahiUiUpdateType::kAnswerLoaded);
  }

  // Verify that `question_answer_view` is visible.
  auto* const question_answer_view =
      panel_widget->GetContentsView()->GetViewByID(
          mahi_constants::ViewId::kQuestionAnswerView);
  ASSERT_TRUE(question_answer_view);
  EXPECT_TRUE(question_answer_view->GetVisible());

  // Verify the question label.
  ASSERT_EQ(question_answer_view->children().size(), 2u);
  EXPECT_EQ(views::AsViewClass<views::Label>(
                question_answer_view->children()[0]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText(),
            question_text);

  // Verify the answer label.
  EXPECT_EQ(base::UTF16ToUTF8(
                views::AsViewClass<views::Label>(
                    question_answer_view->children()[1]->GetViewByID(
                        mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                    ->GetText()),
            GetMahiDefaultTestAnswer());
}

// PendingConsentStatusMahiUiBrowserTest ---------------------------------------

// Tests Mahi UI features when the consent status before the test flow is
// kPending.
class PendingConsentStatusMahiUiBrowserTest : public MahiUiBrowserTestBase {
 private:
  // MahiUiBrowserTestBase:
  void SetUpOnMainThread() override {
    MahiUiBrowserTestBase::SetUpOnMainThread();
    ApplyHMRConsentStatusAndWait(
        chromeos::HMRConsentStatus::kPendingDisclaimer);
  }

  base::test::ScopedFeatureList feature_list_;
};

// Checks that the settings is accessible when the consent status is pending.
IN_PROC_BROWSER_TEST_F(PendingConsentStatusMahiUiBrowserTest,
                       OnContextMenuClickedSettings) {
  // Ensure the Settings app installed.
  WaitForTestSystemAppInstall();

  // Open the Mahi menu by mouse right click on the web contents.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();
  views::Widget* const mahi_menu_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::mahi::MahiMenuView::GetWidgetName());
  ASSERT_TRUE(mahi_menu_widget);

  const views::View* const settings_button =
      mahi_menu_widget->GetContentsView()->GetViewByID(
          chromeos::mahi::ViewID::kSettingsButton);
  ASSERT_TRUE(settings_button);

  // Mouse click `settings_button` of the Mahi menu.
  event_generator().MoveMouseTo(
      settings_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  WaitForSettingsToLoad();

  // Verify that the Settings page is opened in a new window.
  const Browser* const settings_browser =
      chrome::SettingsWindowManager::GetInstance()->FindBrowserForProfile(
          browser()->profile());
  ASSERT_TRUE(settings_browser);
  EXPECT_NE(browser(), settings_browser);
  EXPECT_EQ(
      GURL(chrome::GetOSSettingsUrl(std::string())),
      settings_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// MahiUiWithDisclaimerViewBrowserTest -----------------------------------------

class MahiUiWithDisclaimerViewBrowserTest
    : public PendingConsentStatusMahiUiBrowserTest,
      public WithParamInterface</*accept=*/bool> {};

INSTANTIATE_TEST_SUITE_P(All,
                         MahiUiWithDisclaimerViewBrowserTest,
                         /*accept=*/::testing::Bool());

IN_PROC_BROWSER_TEST_P(MahiUiWithDisclaimerViewBrowserTest,
                       OnContextMenuClickedSummary) {
  EXPECT_FALSE(FindWidgetWithName(MahiPanelWidget::GetName()));

  // Open the Mahi menu by mouse right click on the web contents.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();
  views::Widget* const mahi_menu_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::mahi::MahiMenuView::GetWidgetName());
  ASSERT_TRUE(mahi_menu_widget);

  ui::ScopedAnimationDurationScaleMode zero_duration(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Show the disclaimer view by left clicking the menu's summary button.
  const views::View* const summary_button =
      mahi_menu_widget->GetContentsView()->GetViewByID(
          chromeos::mahi::ViewID::kSummaryButton);
  ASSERT_TRUE(summary_button);
  event_generator().MoveMouseTo(
      summary_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  WaitUntilMahiMenuClosed(mahi_menu_widget);
  EXPECT_TRUE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));

  const bool accept = GetParam();
  ClickDisclaimerViewButton(accept);

  // If user clicks the declination button, the Mahi panel should not show.
  if (!accept) {
    EXPECT_FALSE(FindWidgetWithName(MahiPanelWidget::GetName()));
    return;
  }

  // The code below checks the Mahi panel.

  WaitUntilUiUpdateReceived(MahiUiUpdateType::kSummaryLoaded);
  views::Widget* panel_widget =
      FindWidgetWithNameAndWaitIfNeeded(MahiPanelWidget::GetName());
  ASSERT_TRUE(panel_widget);

  const auto* const summary_label = views::AsViewClass<views::Label>(
      panel_widget->GetContentsView()->GetViewByID(
          mahi_constants::ViewId::kSummaryLabel));
  ASSERT_TRUE(summary_label);
  EXPECT_EQ(base::UTF16ToUTF8(summary_label->GetText()),
            GetMahiDefaultTestSummary());
}

IN_PROC_BROWSER_TEST_P(MahiUiWithDisclaimerViewBrowserTest,
                       OnContextMenuQuestionSent) {
  EXPECT_FALSE(FindWidgetWithName(MahiPanelWidget::GetName()));

  // Open the Mahi menu by mouse right click on the web contents.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();
  views::Widget* const mahi_menu_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::mahi::MahiMenuView::GetWidgetName());
  ASSERT_TRUE(mahi_menu_widget);

  const std::u16string question_text(u"question");
  TypeStringToMahiMenuTextfield(mahi_menu_widget, question_text);

  ui::ScopedAnimationDurationScaleMode zero_duration(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  const views::View* question_submit_button =
      mahi_menu_widget->GetContentsView()->GetViewByID(
          chromeos::mahi::ViewID::kSubmitQuestionButton);
  ASSERT_TRUE(question_submit_button);

  // Mouse click on `question_submit_button`.
  event_generator().MoveMouseTo(
      question_submit_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  WaitUntilMahiMenuClosed(mahi_menu_widget);
  EXPECT_TRUE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));

  const bool accept = GetParam();
  ClickDisclaimerViewButton(accept);

  // If user clicks the declination button, the Mahi panel should not show.
  if (!accept) {
    EXPECT_FALSE(FindWidgetWithName(MahiPanelWidget::GetName()));
    return;
  }

  // The code below checks the Mahi panel.

  WaitUntilUiUpdateReceived(MahiUiUpdateType::kAnswerLoaded);
  views::Widget* panel_widget =
      FindWidgetWithNameAndWaitIfNeeded(MahiPanelWidget::GetName());
  ASSERT_TRUE(panel_widget);

  // Verify that `question_answer_view` is visible.
  auto* const question_answer_view =
      panel_widget->GetContentsView()->GetViewByID(
          mahi_constants::ViewId::kQuestionAnswerView);
  ASSERT_TRUE(question_answer_view);
  EXPECT_TRUE(question_answer_view->GetVisible());

  // Verify the question label.
  ASSERT_EQ(question_answer_view->children().size(), 2u);
  EXPECT_EQ(views::AsViewClass<views::Label>(
                question_answer_view->children()[0]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText(),
            question_text);

  // Verify the answer label.
  EXPECT_EQ(base::UTF16ToUTF8(
                views::AsViewClass<views::Label>(
                    question_answer_view->children()[1]->GetViewByID(
                        mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                    ->GetText()),
            GetMahiDefaultTestAnswer());
}

}  // namespace ash
