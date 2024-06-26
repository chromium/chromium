// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/mahi/fake_mahi_manager.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "ash/system/mahi/test/mock_mahi_ui_controller_delegate.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/components/mahi/public/cpp/mahi_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
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

FakeMahiManager* GetMahiManager() {
  return static_cast<FakeMahiManager*>(chromeos::MahiManager::Get());
}

MahiUiController* GetMahiUiController() {
  return GetMahiManager()->ui_controller();
}

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

// Waits until the specified `MahiUiUpdate` is received.
void WaitUntilUiUpdateReceived(MahiUiUpdateType target_type) {
  NiceMock<MockMahiUiControllerDelegate> mock_controller_delegate(
      GetMahiUiController());

  base::RunLoop run_loop;
  ON_CALL(mock_controller_delegate, OnUpdated)
      .WillByDefault([&run_loop, target_type](const MahiUiUpdate& update) {
        if (update.type() == target_type) {
          run_loop.Quit();
        }
      });
  run_loop.Run();
}

}  // namespace

class MahiUiBrowserTest : public SystemWebAppBrowserTestBase {
 protected:
  ui::test::EventGenerator& event_generator() { return *event_generator_; }

 private:
  // SystemWebAppBrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SystemWebAppBrowserTestBase::SetUpCommandLine(command_line);

    command_line->AppendSwitch(chromeos::switches::kUseFakeMahiManager);
  }

  void SetUpOnMainThread() override {
    SystemWebAppBrowserTestBase::SetUpOnMainThread();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::GetPrimaryRootWindow());
  }

  base::test::ScopedFeatureList feature_list_{chromeos::features::kMahi};
  ScopedFakeMahiManagerZeroDuration scoped_zero_duration_;
  base::AutoReset<bool> ignore_mahi_secret_key_ =
      ash::switches::SetIgnoreMahiSecretKeyForTest();
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
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

  {
    // Wait until the Settings app finishes loading.
    ui_test_utils::AllBrowserTabAddedWaiter waiter;
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        browser()->profile());
    auto* const web_contents = waiter.Wait();
    ASSERT_TRUE(web_contents);
    content::WaitForLoadStop(web_contents);
  }

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

  const std::u16string summary_text(u"sample summary");
  GetMahiManager()->set_summary_text(summary_text);

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
  EXPECT_EQ(data->text(), base::UTF16ToASCII(summary_text));
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

  const views::View* const textfield =
      mahi_menu_widget->GetContentsView()->GetViewByID(
          chromeos::mahi::ViewID::kTextfield);
  ASSERT_TRUE(textfield);

  // Ensure focus on `textfield`.
  event_generator().MoveMouseTo(textfield->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  // Type `question_text`.
  const std::u16string question_text(u"question");
  for (char16_t c : question_text) {
    event_generator().PressAndReleaseKey(
        static_cast<ui::KeyboardCode>(ui::VKEY_A + c - u'a'));
  }

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
  ASSERT_EQ(2u, question_answer_view->children().size());
  EXPECT_EQ(views::AsViewClass<views::Label>(
                question_answer_view->children()[0]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText(),
            question_text);

  // Verify the answer label.
  EXPECT_EQ(views::AsViewClass<views::Label>(
                question_answer_view->children()[1]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText(),
            u"Fake answer");
}

}  // namespace ash
