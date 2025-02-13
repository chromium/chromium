// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_FRE_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_GLIC_FRE_CONTROLLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/auth_controller.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"

class Browser;
class Profile;
namespace views {
class Widget;
}

namespace content {
class WebContents;
}

namespace version_info {
enum class Channel;
}

namespace glic {

class GlicFreDialogView;

// This class owns and manages the glic FRE modal dialog, and is owned by a
// GlicWindowController.
class GlicFreController {
 public:
  GlicFreController(const GlicFreController&) = delete;
  GlicFreController& operator=(const GlicFreController&) = delete;

  GlicFreController(Profile* profile,
                    signin::IdentityManager* identity_manager);
  ~GlicFreController();

  // Close any windows and destroy web contents.
  void Shutdown();

  // Returns whether the FRE dialog should be shown.
  bool ShouldShowFreDialog();

  // Returns whether the FRE dialog can be shown. This function also checks
  // `TabInterface::CanShowModalUI`, which is a mandatory precondition to
  // showing the dialog.
  bool CanShowFreDialog(Browser* browser);

  // Shows the FRE dialog. This should only be called if `ShouldShowFreDialog`
  // and `CanShowFreDialog` are both satisfied.
  void ShowFreDialog(Browser* browser);

  // Closes the FRE dialog if it is open on the active tab of `browser`.
  void DismissFreIfOpenOnActiveTab(Browser* browser);

  // Closes the FRE dialog and immediately opens a glic window attached to
  // the same browser.
  void AcceptFre();

  // Closes the FRE dialog.
  void DismissFre();

  // Returns the WebContents from the dialog view.
  content::WebContents* GetWebContents();

  bool IsShowingDialogForTesting() const;

  base::WeakPtr<GlicFreController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(GlicFreControllerTest,
                           UpdateLauncherOnFreCompletion);
  void ShowFreDialogAfterAuthCheck(base::WeakPtr<Browser> browser,
                                   AuthController::BeforeShowResult result);
  static void OnCheckIsDefaultBrowserFinished(
      version_info::Channel channel,
      shell_integration::DefaultWebClientState state);

  // Called when the tab showing the FRE dialog is detached.
  void OnTabShowingModalWillDetach(tabs::TabInterface* tab,
                                   tabs::TabInterface::DetachReason reason);

  raw_ptr<Profile> profile_;
  std::unique_ptr<views::Widget> fre_widget_;
  raw_ptr<GlicFreDialogView> fre_view_;
  bool first_time_pref_check_done_ = false;
  AuthController auth_controller_;

  // Tracks the tab that the FRE dialog is shown on.
  raw_ptr<tabs::TabInterface> tab_showing_modal_;
  base::CallbackListSubscription will_detach_subscription_;

  base::WeakPtrFactory<GlicFreController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_FRE_CONTROLLER_H_
