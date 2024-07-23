// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/exit_type_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/session_crashed_bubble_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_value_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"

namespace {

// Generates events that result in the button triggering the appropriate click
// action.
void ClickButton(views::BubbleDialogDelegate* crash_bubble_delegate,
                 views::Button* button) {
  crash_bubble_delegate->ResetViewShownTimeStampForTesting();
  gfx::Point center(button->width() / 2, button->height() / 2);
  const ui::MouseEvent event(ui::EventType::kMousePressed, center, center,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  button->OnMousePressed(event);
  button->OnMouseReleased(event);
}

// Urls used for testing.
GURL GetUrl1() {
  return ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("session_history"),
      base::FilePath().AppendASCII("bot1.html"));
}

GURL GetUrl2() {
  return ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("session_history"),
      base::FilePath().AppendASCII("bot2.html"));
}

GURL GetUrl3() {
  return ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("session_history"),
      base::FilePath().AppendASCII("bot3.html"));
}

void WaitForBrowserToFinishLoading(Browser* browser) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    contents->GetController().LoadIfNecessary();
    content::WaitForLoadStop(contents);
  }
}

}  // namespace

class ExitTypeServiceTest : public InProcessBrowserTest {
 protected:
  ExitType GetLastSessionExitType() {
    return GetExitTypeService()->last_session_exit_type();
  }

  bool IsSessionServiceSavingEnabled() {
    return SessionServiceFactory::GetForProfile(browser()->profile())
        ->is_saving_enabled();
  }

  ExitTypeService* GetExitTypeService() {
    return ExitTypeService::GetInstanceForProfile(browser()->profile());
  }
};

// Sets state so that on the next run the last session exit type is crashed.
IN_PROC_BROWSER_TEST_F(ExitTypeServiceTest, PRE_PRE_PRE_CrashCrashNewBrowser) {
  ExitTypeService::GetInstanceForProfile(browser()->profile())
      ->SetWaitingForUserToAckCrashForTest(true);
  SessionStartupPref::SetStartupPref(
      browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
}

IN_PROC_BROWSER_TEST_F(ExitTypeServiceTest, PRE_PRE_CrashCrashNewBrowser) {
  ASSERT_EQ(ExitType::kCrashed, GetLastSessionExitType());
  EXPECT_FALSE(IsSessionServiceSavingEnabled());
}

// As the user didn't ack the crash, last session exit type should still be
// crashed.
IN_PROC_BROWSER_TEST_F(ExitTypeServiceTest, PRE_CrashCrashNewBrowser) {
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(ExitType::kCrashed, GetLastSessionExitType());
  EXPECT_FALSE(IsSessionServiceSavingEnabled());
  // As the crashed bubble is still open, creating a tab in the existing
  // browser should not enable saving.
  chrome::NewTab(browser());
  // Creating a new browser should enable saving.
  CreateBrowser(browser()->profile());
  EXPECT_TRUE(IsSessionServiceSavingEnabled());
  chrome::AttemptUserExit();
}

// And lastly, the two browsers previously open should be restored and saving
// should be enabled (because the previous PRE_ test cleanly shutdown).
IN_PROC_BROWSER_TEST_F(ExitTypeServiceTest, CrashCrashNewBrowser) {
  ASSERT_EQ(ExitType::kClean, GetLastSessionExitType());
  EXPECT_TRUE(IsSessionServiceSavingEnabled());
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());
}

// Creates two browsers navigating to a couple of urls and sets it so on next
// run last session exit status is crashed.
IN_PROC_BROWSER_TEST_F(ExitTypeServiceTest, PRE_PRE_RestoreFromCrashBubble) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));
  Browser* browser2 = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, GetUrl2()));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, GetUrl3()));
  ExitTypeService::GetInstanceForProfile(browser()->profile())
      ->SetWaitingForUserToAckCrashForTest(true);
}

// Verify saving is not enabled, because the crash bubble is still visible.
IN_PROC_BROWSER_TEST_F(ExitTypeServiceTest, PRE_RestoreFromCrashBubble) {
  ASSERT_EQ(ExitType::kCrashed, GetLastSessionExitType());
  EXPECT_FALSE(IsSessionServiceSavingEnabled());
}

// Trigger restore, which should enable saving.
IN_PROC_BROWSER_TEST_F(ExitTypeServiceTest, RestoreFromCrashBubble) {
  ASSERT_EQ(ExitType::kCrashed, GetLastSessionExitType());
  EXPECT_FALSE(IsSessionServiceSavingEnabled());

  views::BubbleDialogDelegate* crash_bubble_delegate =
      SessionCrashedBubbleView::GetInstanceForTest();
  ASSERT_TRUE(crash_bubble_delegate);
  ClickButton(crash_bubble_delegate, crash_bubble_delegate->GetOkButton());
  ASSERT_TRUE(SessionRestore::IsRestoring(browser()->profile()));
  EXPECT_TRUE(GetExitTypeService()->waiting_for_user_to_ack_crash());
  base::RunLoop run_loop;
  GetExitTypeService()->AddCrashAckCallback(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(IsSessionServiceSavingEnabled());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const bool restores_to_initial_browser = false;
#else
  const bool restores_to_initial_browser = true;
#endif
  ASSERT_EQ(2u + (restores_to_initial_browser ? 0u : 1u),
            BrowserList::GetInstance()->size());
  Browser* browser1 =
      BrowserList::GetInstance()->get(restores_to_initial_browser ? 0 : 1);
  Browser* browser2 =
      BrowserList::GetInstance()->get(restores_to_initial_browser ? 1 : 2);
  ASSERT_EQ((restores_to_initial_browser ? 2 : 1),
            browser1->tab_strip_model()->count());
  WaitForBrowserToFinishLoading(browser1);
  // The first tab is created during startup.
  if (restores_to_initial_browser) {
    EXPECT_EQ(GURL("about:blank"),
              browser1->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  }
  EXPECT_EQ(GetUrl1(),
            browser1->tab_strip_model()
                ->GetWebContentsAt(restores_to_initial_browser ? 1 : 0)
                ->GetURL());
  ASSERT_EQ(2, browser2->tab_strip_model()->count());
  WaitForBrowserToFinishLoading(browser2);
  EXPECT_EQ(GetUrl2(),
            browser2->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GetUrl3(),
            browser2->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}

// Marks the profile as crashing.
IN_PROC_BROWSER_TEST_F(ExitTypeServiceTest, PRE_CloseCrashBubbleEnablesSaving) {
  ExitTypeService::GetInstanceForProfile(browser()->profile())
      ->SetWaitingForUserToAckCrashForTest(true);
}

// TODO(crbug.com/40927197): Re-enable test that flakily times out
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CloseCrashBubbleEnablesSaving \
  DISABLED_CloseCrashBubbleEnablesSaving
#else
#define MAYBE_CloseCrashBubbleEnablesSaving CloseCrashBubbleEnablesSaving
#endif
// Closes the crash bubble, which should enable saving.
IN_PROC_BROWSER_TEST_F(ExitTypeServiceTest,
                       MAYBE_CloseCrashBubbleEnablesSaving) {
  ASSERT_EQ(ExitType::kCrashed, GetLastSessionExitType());
  EXPECT_FALSE(IsSessionServiceSavingEnabled());

  views::BubbleDialogDelegate* crash_bubble_delegate =
      SessionCrashedBubbleView::GetInstanceForTest();
  ASSERT_TRUE(crash_bubble_delegate);
  base::RunLoop run_loop;
  GetExitTypeService()->AddCrashAckCallback(run_loop.QuitClosure());
  crash_bubble_delegate->GetBubbleFrameView()->GetWidget()->Close();
  EXPECT_FALSE(SessionRestore::IsRestoring(browser()->profile()));
  run_loop.Run();
  EXPECT_FALSE(GetExitTypeService()->waiting_for_user_to_ack_crash());
  EXPECT_TRUE(IsSessionServiceSavingEnabled());
}
