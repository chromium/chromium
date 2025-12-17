// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_FRE_GLIC_FRE_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_FRE_GLIC_FRE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic.mojom.h"
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
class GlicFrePageHandler;

// This enum is used to record the reason for the FRE error state.
// These values are persisted to logs.
// LINT.IfChange(FreErrorStateReason)
enum class FreErrorStateReason {
  // Sign-in is required.
  kSignInRequired = 0,
  // Error while re-syncing cookies before showing FRE.
  kErrorResyncingCookies = 1,
  // Timeout exceeded during loading error.
  kTimeoutExceeded = 2,
  kMaxValue = kTimeoutExceeded,
};
// LINT.ThenChange(tools/metrics/histograms/metadata/glic/enums.xml:FreErrorStateReason)

// This enum is used for the Glic.Fre.WidgetClosedReason2 histogram.
// It mirrors views::Widget::ClosedReason and adds Glic-specific reasons.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(GlicFreWidgetClosedReason)
enum class GlicFreWidgetClosedReason {
  kUnspecified = 0,
  kEscKeyPressed = 1,
  kCloseButtonClicked = 2,
  kLostFocus = 3,
  kCancelButtonClicked = 4,
  kAcceptButtonClicked = 5,
  kHostTabClosed = 6,
  kHostTabMoved = 7,
  kMaxValue = kHostTabMoved,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicFreWidgetClosedReason)

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
  void OpenFreDialogInNewTab(BrowserWindowInterface* bwi,
                             mojom::InvocationSource source);

  // Shows the FRE dialog. This should only be called if `ShouldShowFreDialog`
  // and `CanShowFreDialog` are both satisfied.
  void ShowFreDialog(Browser* browser, mojom::InvocationSource source);

  // Closes the FRE dialog if it is open on the active tab of `browser`.
  void DismissFreIfOpenOnActiveTab(Browser* browser);

  // Closes the FRE dialog and immediately opens a glic window attached to
  // the same browser.
  // |handler| is the specific PageHandler that triggered the acceptance.
  void AcceptFre(GlicFrePageHandler* handler);

  // Rejects the FRE dialog.
  void RejectFre();

  // Closes the FRE dialog.
  void DismissFre(mojom::FreWebUiState panel);

  // Used when the native window is closed directly.
  void CloseWithReason(views::Widget::ClosedReason reason);

  // Re-sync cookies to FRE webview.
  void PrepareForClient(base::OnceCallback<void(bool)> callback);

  // Loading timeout was exceeded.
  void ExceededTimeoutError();

  // Notify FRE controller that the user clicked on a link.
  void OnLinkClicked(const GURL& url);

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

  bool IsShowingDialogAndStateInitialized() const;

  gfx::Size GetFreInitialSize();

  void UpdateFreWidgetSize(const gfx::Size& new_size);

  AuthController& GetAuthControllerForTesting() { return auth_controller_; }

  Profile* profile() { return profile_; }

  base::WeakPtr<GlicFreController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetIsShowingDialogForTesting(bool is_showing) {
    is_showing_dialog_for_testing_ = is_showing;
  }

  void RecordFrameworkStartTime();

  struct InitTimestamps {
    base::TimeTicks open_start_time;
    base::TimeTicks framework_start_time;
  };

  // Registers a new PageHandler. Called when a new FRE UI instance is created.
  // Returns the start time of the request to show the FRE.
  InitTimestamps RegisterPageHandler(GlicFrePageHandler* handler);

  // Unregisters a PageHandler. Called when an FRE UI instance is destroyed.
  void UnregisterPageHandler(GlicFrePageHandler* handler);

  // Called when the user opens the FRE in dialog or sidepanel. This sets the
  // value of `pending_open_start_time_`.
  void MarkFreStartAttempt();

  // Logs when the FRE in sidepanel is shown.
  void MarkSidepanelFreShown();

 private:
  void ShowFreDialogAfterAuthCheck(base::WeakPtr<Browser> browser,
                                   mojom::InvocationSource source);

  // Called when the tab showing the FRE dialog is detached.
  void OnTabShowingModalWillDetach(tabs::TabInterface* tab,
                                   tabs::TabInterface::DetachReason reason);

  void CreateView();

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

  // Used to track the total time this specific FRE instance has been open. This
  // value is set when the FRE is toggled on.
  std::optional<base::TimeTicks> pending_open_start_time_;

  // Used to track the time between the start of the WebUI framework loading and
  // the moment it's fully loaded. This is logged before the WebUI controller is
  // created.
  std::optional<base::TimeTicks> pending_framework_start_time_;

  std::optional<bool> is_showing_dialog_for_testing_;

  // List of active PageHandlers (one per FRE UI instance).
  // Safe because GlicFrePageHandler explicitly calls UnregisterPageHandler in
  // its destructor, ensuring pointers are removed before invalidation.
  std::vector<raw_ptr<GlicFrePageHandler>> handlers_;

  base::WeakPtrFactory<GlicFreController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_FRE_GLIC_FRE_CONTROLLER_H_
