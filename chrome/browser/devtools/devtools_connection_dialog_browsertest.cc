// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_connection_dialog.h"

#include "base/test/test_future.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

class DevToolsConnectionDialogBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(DevToolsConnectionDialogBrowserTest, NullBrowser) {
  base::test::TestFuture<
      content::DevToolsManagerDelegate::AcceptConnectionResult>
      future;
  DevToolsConnectionDialog::Show(nullptr, future.GetCallback());
  EXPECT_EQ(future.Get(),
            content::DevToolsManagerDelegate::AcceptConnectionResult::kDeny);
}

IN_PROC_BROWSER_TEST_F(DevToolsConnectionDialogBrowserTest, Accept) {
  base::test::TestFuture<
      content::DevToolsManagerDelegate::AcceptConnectionResult>
      future;
  auto* dialog =
      DevToolsConnectionDialog::Show(browser(), future.GetCallback());
  views::Widget* widget = dialog->GetDialogWidgetForTesting().get();
  ASSERT_TRUE(widget);

  views::DialogDelegate* dialog_delegate =
      widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);

  dialog_delegate->AcceptDialog();

  EXPECT_EQ(future.Get(),
            content::DevToolsManagerDelegate::AcceptConnectionResult::kAllow);
}

IN_PROC_BROWSER_TEST_F(DevToolsConnectionDialogBrowserTest, Cancel) {
  base::test::TestFuture<
      content::DevToolsManagerDelegate::AcceptConnectionResult>
      future;
  auto* dialog =
      DevToolsConnectionDialog::Show(browser(), future.GetCallback());
  views::Widget* widget = dialog->GetDialogWidgetForTesting().get();
  ASSERT_TRUE(widget);

  views::DialogDelegate* dialog_delegate =
      widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);

  dialog_delegate->CancelDialog();

  EXPECT_EQ(future.Get(),
            content::DevToolsManagerDelegate::AcceptConnectionResult::kDeny);
}

IN_PROC_BROWSER_TEST_F(DevToolsConnectionDialogBrowserTest, Close) {
  base::test::TestFuture<
      content::DevToolsManagerDelegate::AcceptConnectionResult>
      future;
  auto* dialog =
      DevToolsConnectionDialog::Show(browser(), future.GetCallback());
  views::Widget* widget = dialog->GetDialogWidgetForTesting().get();
  ASSERT_TRUE(widget);

  widget->Close();

  EXPECT_EQ(future.Get(),
            content::DevToolsManagerDelegate::AcceptConnectionResult::kDeny);
}

IN_PROC_BROWSER_TEST_F(DevToolsConnectionDialogBrowserTest, Disable) {
  base::test::TestFuture<
      content::DevToolsManagerDelegate::AcceptConnectionResult>
      future;

  auto* dialog =
      DevToolsConnectionDialog::Show(browser(), future.GetCallback());
  auto* widget = dialog->GetDialogWidgetForTesting().get();
  ASSERT_TRUE(widget);

  views::BubbleDialogModelHost* dialog_model_dialog =
      static_cast<views::BubbleDialogModelHost*>(widget->widget_delegate());
  ASSERT_TRUE(dialog_model_dialog);

  content::WebContentsAddedObserver new_tab_observer;

  views::test::ButtonTestApi(
      static_cast<views::MdTextButton*>(dialog_model_dialog->GetExtraView()))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(future.Get(),
            content::DevToolsManagerDelegate::AcceptConnectionResult::kDeny);

  content::WebContents* new_web_contents = new_tab_observer.GetWebContents();
  content::TestNavigationObserver(new_web_contents).Wait();
  EXPECT_EQ(new_web_contents->GetVisibleURL(),
            "chrome://inspect/#remote-debugging");
}
