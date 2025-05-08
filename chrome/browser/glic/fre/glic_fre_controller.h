// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_FRE_GLIC_FRE_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_FRE_GLIC_FRE_CONTROLLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/shell_integration.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/widget/widget.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace version_info {
enum class Channel;
}

namespace views {
class Widget;
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

  mojom::FreWebUiState GetWebUiState() const { return webui_state_; }
  void WebUiStateChanged(mojom::FreWebUiState new_state);

  using WebUiStateChangedCallback =
      base::RepeatingCallback<void(mojom::FreWebUiState new_state)>;

  // Registers |callback| to be called whenever the WebUi state changes.
  base::CallbackListSubscription AddWebUiStateChangedCallback(
      WebUiStateChangedCallback callback);

  // Close any windows and destroy web contents.
  void Shutdown();

  // Returns whether the FRE dialog should be shown.
  bool ShouldShowFreDialog();

  // Returns whether the FRE dialog can be shown. This function also checks
  // `TabInterface::CanShowModalUI`, which is a mandatory precondition to
  // showing the dialog.
  bool CanShowFreDialog(Browser* browser);

  // Open the new tab page in the browser and show the FRE in that tab if
  // possible.
  void OpenFreDialogInNewTab(BrowserWindowInterface* bwi);

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

  // Used when the native window is closed directly.
  void CloseWithReason(views::Widget::ClosedReason reason);

  // Re-sync cookies to FRE webview.
  void PrepareForClient(base::OnceCallback<void(bool)> callback);

  // Notify FRE controller that the user clicked on a link.
  void OnLinkClicked(const GURL& url);

  // Notify FRE controller that the user clicked "no thanks" in the FRE.
  void OnNoThanksClicked();

  // Attempts to warm the FRE web contents.
  void TryPreload();

  // Returns true if the FRE web contents are loaded (either because it has been
  // preloaded or because it is visible).
  bool IsWarmed() const;

  // Returns the WebContents from the dialog view.
  content::WebContents* GetWebContents();

  // Preconnect to the server that hosts the FRE, so that it loads faster.
  // Does nothing if the FRE should not be shown.
  void MaybePreconnect();

  bool IsShowingDialog() const;

  gfx::Size GetFreInitialSize();

  void UpdateFreWidgetSize(const gfx::Size& new_size);

  AuthController& GetAuthControllerForTesting() { return auth_controller_; }

  base::WeakPtr<GlicFreController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(GlicFreControllerTest,
                           UpdateLauncherOnFreCompletion);
  void ShowFreDialogAfterAuthCheck(base::WeakPtr<Browser> browser);
  static void OnCheckIsDefaultBrowserFinished(
      version_info::Channel channel,
      shell_integration::DefaultWebClientState state);

  // Called when the tab showing the FRE dialog is detached.
  void OnTabShowingModalWillDetach(tabs::TabInterface* tab,
                                   tabs::TabInterface::DetachReason reason);

  void CreateView();

  void RecordMetricsIfDialogIsShowingAndReady();

  raw_ptr<Profile> const profile_;
  std::unique_ptr<views::Widget> fre_widget_;
  std::unique_ptr<GlicFreDialogView> fre_view_;
  // This is owned by the GlicFreDialogView but we retain a pointer to it so
  // that we can continue to reference it even after `fre_view_` relinquishes
  // ownership to the widget.
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  AuthController auth_controller_;

  // The invocation source browser.
  raw_ptr<Browser> source_browser_ = nullptr;

  // Tracks the tab that the FRE dialog is shown on.
  raw_ptr<tabs::TabInterface> tab_showing_modal_;
  base::CallbackListSubscription will_detach_subscription_;

  mojom::FreWebUiState webui_state_ = mojom::FreWebUiState::kUninitialized;
  // List of callbacks to be notified when webui state has changed.
  base::RepeatingCallbackList<void(mojom::FreWebUiState)>
      webui_state_callback_list_;

  // The timestamp when the FRE window is shown.
  base::TimeTicks show_start_time_;

  base::WeakPtrFactory<GlicFreController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_FRE_GLIC_FRE_CONTROLLER_H_
