// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/captive_portal_dialog_delegate.h"

#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_display_host_mojo.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {
namespace {

// A simulated modal dialog. Taking focus seems important to repro the crash,
// but I'm not sure why.
class ChildModalDialogDelegate : public views::DialogDelegateView {
 public:
  ChildModalDialogDelegate() {
    // Our views::Widget will delete us.
    DCHECK(owned_by_widget());
    SetModalType(ui::MODAL_TYPE_CHILD);
    SetFocusBehavior(FocusBehavior::ALWAYS);
    // Dialogs that take focus must have a name and role to pass accessibility
    // checks.
    GetViewAccessibility().OverrideRole(ax::mojom::Role::kDialog);
    GetViewAccessibility().OverrideName("Test dialog");
  }
  ChildModalDialogDelegate(const ChildModalDialogDelegate&) = delete;
  ChildModalDialogDelegate& operator=(const ChildModalDialogDelegate&) = delete;
  ~ChildModalDialogDelegate() override = default;
};

class CaptivePortalDialogDelegateTest : public LoginManagerTest {
 public:
  // Simulate a login screen with an existing user.
  CaptivePortalDialogDelegateTest() { login_mixin_.AppendRegularUsers(1); }
  CaptivePortalDialogDelegateTest(const CaptivePortalDialogDelegateTest&) =
      delete;
  CaptivePortalDialogDelegateTest& operator=(
      const CaptivePortalDialogDelegateTest&) = delete;
  ~CaptivePortalDialogDelegateTest() override = default;

  LoginManagerMixin login_mixin_{&mixin_host_};
};

// Regression test for use-after-free and crash. https://1170577
IN_PROC_BROWSER_TEST_F(CaptivePortalDialogDelegateTest,
                       ShowModalDialogDoesNotCrash) {
  // Show the captive portal dialog.
  LoginDisplayHostMojo* login_display_host =
      static_cast<LoginDisplayHostMojo*>(LoginDisplayHost::default_host());
  OobeUIDialogDelegate* oobe_ui_dialog =
      login_display_host->EnsureDialogForTest();
  CaptivePortalDialogDelegate* portal_dialog =
      oobe_ui_dialog->captive_portal_delegate_for_test();
  views::test::WidgetVisibleWaiter show_waiter(
      portal_dialog->widget_for_test());
  portal_dialog->Show();
  show_waiter.Wait();

  // Show a child modal dialog, similar to an http auth modal dialog.
  content::WebContents* web_contents = portal_dialog->web_contents_for_test();
  // The ChildModalDialogDelegate is owned by the views system.
  constrained_window::ShowWebModalDialogViews(new ChildModalDialogDelegate,
                                              web_contents);

  // Close the parent dialog.
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      portal_dialog->widget_for_test());
  portal_dialog->Close();
  destroyed_waiter.Wait();

  // No crash.
}

}  // namespace
}  // namespace ash
