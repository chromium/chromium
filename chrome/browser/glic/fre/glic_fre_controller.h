// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_FRE_GLIC_FRE_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_FRE_GLIC_FRE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/shell_integration.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace signin {
class IdentityManager;
}

namespace version_info {
enum class Channel;
}

namespace glic {

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

///////////
// WARNING: The FRE dialog is deprecated, this will be removed soon. However,
// some small parts may be still need kept for Unified FRE.
// See b/489122337
///////////

// This class owns and manages the glic FRE modal dialog, and is owned by a
// GlicInstanceCoordinator.
//
// Warning: This class is used both for the FRE dialog and for the Unified
// FRE which has no dialog. Parts of this class which relate to the dialog
// are removed on Android.
class GlicFreController {
 public:
  GlicFreController(const GlicFreController&) = delete;
  GlicFreController& operator=(const GlicFreController&) = delete;

  GlicFreController(Profile* profile,
                    signin::IdentityManager* identity_manager);
  virtual ~GlicFreController();

  void WebUiStateChanged(mojom::FreWebUiState new_state);

  using WebUiStateChangedCallback =
      base::RepeatingCallback<void(mojom::FreWebUiState new_state)>;

  // Registers |callback| to be called whenever the WebUi state changes. Virtual
  // for testing.
  virtual base::CallbackListSubscription AddWebUiStateChangedCallback(
      WebUiStateChangedCallback callback);

  // Closes the FRE dialog and immediately opens a glic window attached to
  // the same browser.
  // |handler| is the specific PageHandler that triggered the acceptance.
  void AcceptFre(GlicFrePageHandler* handler);

  // Rejects the FRE dialog.
  void RejectFre();

  // Re-sync cookies to FRE webview.
  void PrepareForClient(base::OnceCallback<void(bool)> callback);

  // Loading timeout was exceeded.
  void ExceededTimeoutError();

  // Notify FRE controller that the user clicked on a link.
  void OnLinkClicked(const GURL& url);

  void RecordFrameworkStartTime();

  // Registers a new PageHandler. Called when a new FRE UI instance is created.
  // Returns the framework start time.
  base::TimeTicks RegisterPageHandler(GlicFrePageHandler* handler);

  // Unregisters a PageHandler. Called when an FRE UI instance is destroyed.
  void UnregisterPageHandler(GlicFrePageHandler* handler);

 private:
  raw_ptr<Profile> const profile_;

  mojom::FreWebUiState webui_state_ = mojom::FreWebUiState::kUninitialized;
  // List of callbacks to be notified when webui state has changed.
  base::RepeatingCallbackList<void(mojom::FreWebUiState)>
      webui_state_callback_list_;

  // Used to track the time between the start of the WebUI framework loading and
  // the moment it's fully loaded. This is logged before the WebUI controller is
  // created.
  std::optional<base::TimeTicks> pending_framework_start_time_;

  // List of active PageHandlers (one per FRE UI instance).
  // Safe because GlicFrePageHandler explicitly calls UnregisterPageHandler in
  // its destructor, ensuring pointers are removed before invalidation.
  std::vector<raw_ptr<GlicFrePageHandler>> handlers_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_FRE_GLIC_FRE_CONTROLLER_H_
