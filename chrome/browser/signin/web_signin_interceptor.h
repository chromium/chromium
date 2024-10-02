// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_WEB_SIGNIN_INTERCEPTOR_H_
#define CHROME_BROWSER_SIGNIN_WEB_SIGNIN_INTERCEPTOR_H_

#include <memory>
#include <optional>

#include "base/cancelable_callback.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {
class WebContents;
}

struct AccountInfo;
class Browser;

// Outcome of the interception heuristic (decision whether the interception
// bubble is shown or not).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SigninInterceptionHeuristicOutcome {
  // Interception succeeded:
  kInterceptProfileSwitch = 0,
  kInterceptMultiUser = 1,
  kInterceptEnterprise = 2,

  // Interception aborted:
  // This is a "Sync" sign in and not a "web" sign in.
  kAbortSyncSignin = 3,
  // Another interception is already in progress.
  kAbortInterceptInProgress = 4,
  // This is not a new account (reauth).
  kAbortAccountNotNew = 5,
  // New profile is not offered when there is only one account.
  kAbortSingleAccount = 6,
  // Extended account info could not be downloaded.
  kAbortAccountInfoTimeout = 7,
  // Account info not compatible with interception (e.g. same Gaia name).
  kAbortAccountInfoNotCompatible = 8,
  // Profile creation disallowed.
  kAbortProfileCreationDisallowed = 9,
  // The interceptor was shut down before the heuristic completed.
  kAbortShutdown = 10,
  // The interceptor is not offered when  the `WebContents` has no browser
  // associated, or its browser does not support displaying the interception UI.
  kAbortNoSupportedBrowser = 11,
  // A password update is required for the account, and this takes priority over
  // signin interception.
  kAbortPasswordUpdate = 12,
  // A password update will be required for the account: the password used on
  // the form does not match the stored password.
  kAbortPasswordUpdatePending = 13,
  // The user already declined a new profile for this account, the UI is not
  // shown again.
  kAbortUserDeclinedProfileForAccount = 14,
  // Signin interception is disabled by the SigninInterceptionEnabled policy.
  kAbortInterceptionDisabled = 15,

  // Interception succeeded when enteprise account separation is mandatory.
  kInterceptEnterpriseForced = 16,
  kInterceptEnterpriseForcedProfileSwitch = 17,

  // The interceptor is not triggered if the tab has already been closed.
  kAbortTabClosed = 18,

  // Interception succeeded:
  // Interception happens when the first account signs in to the web and no
  // account is yet signed in to the Chrome Profile, the prompt suggests signing
  // in to Chrome.
  kInterceptChromeSignin = 19,

  // Interception aborted:
  // The user signed out while the interception was in progress.
  kAbortSignedOut = 20,
  // This is not the first account in the identity manager but there is no
  // primary account.
  kAbortNotFirstAccountButNoPrimaryAccount = 21,

  kMaxValue = kAbortNotFirstAccountButNoPrimaryAccount,
};

// Returns whether the heuristic outcome is a success (the signin should be
// intercepted).
bool SigninInterceptionHeuristicOutcomeIsSuccess(
    SigninInterceptionHeuristicOutcome outcome);

// User selection in the interception bubble.
enum class SigninInterceptionUserChoice { kAccept, kDecline };

// User action resulting from the interception bubble.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SigninInterceptionResult {
  kAccepted = 0,
  kDeclined = 1,
  // The user did not interact with the intercept. This will be recoreded if the
  // browser was closed without any interaction for example.
  kIgnored = 2,

  // Used when the bubble was not shown because it's not implemented.
  kNotDisplayed = 3,

  // Deprecated(10/23): kAcceptedWithGuest = 4,

  kAcceptedWithExistingProfile = 5,

  // The user dismissed the intercept without an explicit Accept or Decline
  // event, for example by pressing the Escape key.
  kDismissed = 6,

  kMaxValue = kDismissed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SigninInterceptionDismissReason {
  kEscKey = 0,
  kIdentityPillPressed = 1,

  kMaxValue = kIdentityPillPressed,
};

// The ScopedWebSigninInterceptionBubbleHandle closes the signin intercept
// bubble when it is destroyed, if the bubble is still opened. Note that this
// handle does not prevent the bubble from being closed for other reasons.
class ScopedWebSigninInterceptionBubbleHandle {
 public:
  virtual ~ScopedWebSigninInterceptionBubbleHandle() = 0;
};

class WebSigninInterceptor {
 public:
  enum class SigninInterceptionType {
    kProfileSwitch,
    kEnterprise,
    kMultiUser,
    kEnterpriseForced,
    kEnterpriseAcceptManagement,
    kProfileSwitchForced,
    kChromeSignin,
    kEnterpriseOIDC
  };

  // Delegate class responsible for showing the various interception UIs.
  class Delegate {
   public:
    // Parameters for interception bubble UIs.
    struct BubbleParameters {
      BubbleParameters(SigninInterceptionType interception_type,
                       AccountInfo intercepted_account,
                       AccountInfo primary_account,
                       SkColor profile_highlight_color = SkColor(),
                       bool show_link_data_option = false,
                       bool show_managed_disclaimer = false);

      BubbleParameters(const BubbleParameters& copy);
      BubbleParameters& operator=(const BubbleParameters&);
      ~BubbleParameters();

      SigninInterceptionType interception_type;
      AccountInfo intercepted_account;
      AccountInfo primary_account;
      SkColor profile_highlight_color;
      bool show_link_data_option;
      bool show_managed_disclaimer;
    };

    virtual ~Delegate() = default;

    // Returns whether the `web_contents` supports signin interception.
    virtual bool IsSigninInterceptionSupported(
        const content::WebContents& web_contents) = 0;

    // Shows the signin interception bubble and calls |callback| to indicate
    // whether the user should continue in a new profile.
    // The callback is never called if the delegate is deleted before it
    // completes.
    // May return a nullptr handle if the bubble cannot be shown.
    // Warning: the handle closes the bubble when it is destroyed ; it is the
    // responsibility of the caller to keep the handle alive until the bubble
    // should be closed.
    // The callback must not be called synchronously if this function returns a
    // valid handle (because the caller needs to be able to close the bubble
    // from the callback).
    virtual std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
    ShowSigninInterceptionBubble(
        content::WebContents* web_contents,
        const BubbleParameters& bubble_parameters,
        base::OnceCallback<void(SigninInterceptionResult)> callback) = 0;

    virtual std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
    ShowOidcInterceptionDialog(
        content::WebContents* web_contents,
        const BubbleParameters& bubble_parameters,
        signin::SigninChoiceWithConfirmationCallback callback,
        base::OnceClosure dialog_closed_closure,
        base::OnceClosure retry_callback) = 0;

    // Shows the first run experience for `account_id` in `browser` opened for
    // a newly created profile.
    virtual void ShowFirstRunExperienceInNewProfile(
        Browser* browser,
        const CoreAccountId& account_id,
        SigninInterceptionType interception_type) = 0;
  };

  WebSigninInterceptor(const WebSigninInterceptor&) = delete;
  WebSigninInterceptor& operator=(const WebSigninInterceptor&) = delete;

 protected:
  WebSigninInterceptor();
  virtual ~WebSigninInterceptor();
};

#endif  // CHROME_BROWSER_SIGNIN_WEB_SIGNIN_INTERCEPTOR_H_
