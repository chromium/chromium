// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_CONTROLLER_AUTOFILL_DELEGATE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_CONTROLLER_AUTOFILL_DELEGATE_H_

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller_delegate.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/native_widget_types.h"

namespace password_manager {
class PasskeyCredential;
class PasswordCredentialFiller;
class PasswordManagerClient;
class UiCredential;
class WebAuthnCredentialsDelegate;
}  // namespace password_manager

namespace content {
class WebContents;
}

class ChromePasswordManagerClient;
class TouchToFillController;
class Profile;

// Delegate interface for TouchToFillController being used in an autofill
// context.
class TouchToFillControllerAutofillDelegate
    : public TouchToFillControllerDelegate {
 public:
  using ShowHybridOption = base::StrongAlias<struct ShowHybridOptionTag, bool>;
  using ShowPasswordMigrationWarningCallback = base::RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>;

  // The action a user took when interacting with the Touch To Fill sheet.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Needs to stay in sync with
  // TouchToFill.UserAction in enums.xml and UserAction in
  // TouchToFillComponent.java.
  //
  // TODO(crbug.com/1013134): De-duplicate the Java and C++ enum.
  enum class UserAction {
    kSelectedCredential = 0,
    kDismissed = 1,
    kSelectedManagePasswords = 2,
    kSelectedPasskeyCredential = 3,
    kSelectedHybrid = 4,
  };

  // The final outcome that closes the Touch To Fill sheet.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Needs to stay in sync with
  // TouchToFill.Outcome in enums.xml.
  enum class TouchToFillOutcome {
    kCredentialFilled = 0,
    kSheetDismissed = 1,
    kReauthenticationFailed = 2,
    kManagePasswordsSelected = 3,
    kPasskeyCredentialSelected = 4,
    kHybridSignInSelected = 5,
    kMaxValue = kHybridSignInSelected,
  };

  // No-op constructor for tests.
  TouchToFillControllerAutofillDelegate(
      base::PassKey<class TouchToFillControllerAutofillTest>,
      password_manager::PasswordManagerClient* password_client,
      content::WebContents* web_contents,
      scoped_refptr<device_reauth::DeviceAuthenticator> authenticator,
      base::WeakPtr<password_manager::WebAuthnCredentialsDelegate>
          webauthn_delegate,
      std::unique_ptr<password_manager::PasswordCredentialFiller> filler,
      ShowHybridOption should_show_hybrid_option,
      ShowPasswordMigrationWarningCallback show_password_migration_warning);

  TouchToFillControllerAutofillDelegate(
      ChromePasswordManagerClient* password_client,
      scoped_refptr<device_reauth::DeviceAuthenticator> authenticator,
      base::WeakPtr<password_manager::WebAuthnCredentialsDelegate>
          webauthn_delegate,
      std::unique_ptr<password_manager::PasswordCredentialFiller> filler,
      ShowHybridOption should_show_hybrid_option);
  TouchToFillControllerAutofillDelegate(
      const TouchToFillControllerAutofillDelegate&) = delete;
  TouchToFillControllerAutofillDelegate& operator=(
      const TouchToFillControllerAutofillDelegate&) = delete;
  ~TouchToFillControllerAutofillDelegate() override;

  // TouchToFillControllerDelegate:
  void OnShow(base::span<const password_manager::UiCredential> credentials,
              base::span<password_manager::PasskeyCredential>
                  passkey_credentials) override;
  void OnCredentialSelected(const password_manager::UiCredential& credential,
                            base::OnceClosure action_completed) override;
  void OnPasskeyCredentialSelected(
      const password_manager::PasskeyCredential& credential,
      base::OnceClosure action_completed) override;
  void OnManagePasswordsSelected(bool passkeys_shown,
                                 base::OnceClosure action_completed) override;
  void OnHybridSignInSelected(base::OnceClosure action_completed) override;
  void OnDismiss(base::OnceClosure action_completed) override;
  const GURL& GetFrameUrl() override;
  bool ShouldTriggerSubmission() override;
  bool ShouldShowHybridOption() override;
  gfx::NativeView GetNativeView() override;

 private:
  // Called after the biometric reauth completes. If `authSuccessful` is
  // true, `credential` will be filled into the form.
  void OnReauthCompleted(password_manager::UiCredential credential,
                         bool authSuccessful);

  // Fills the credential into the form.
  void FillCredential(const password_manager::UiCredential& credential);

  // Called upon completion or dismissal to perform cleanup.
  void CleanUpFillerAndReportOutcome(TouchToFillOutcome outcome,
                                     bool show_virtual_keyboard);

  void ShowPasswordMigrationWarningIfNeeded();

  // Callback to the controller to be invoked when a finalizing action has
  // completed. This will result in the destruction of the delegate so
  // no internal state should be touched after its invocation.
  base::OnceClosure action_complete_;

  // Weak pointer to the PasswordManagerClient this class is tied to.
  raw_ptr<password_manager::PasswordManagerClient> password_client_ = nullptr;

  raw_ptr<content::WebContents> web_contents_;

  // Authenticator used to trigger a biometric auth before filling.
  scoped_refptr<device_reauth::DeviceAuthenticator> authenticator_;

  // Weak pointer to WebAuthnCredentialsDelegate to select passkeys or start
  // hybrid sign in.
  base::WeakPtr<password_manager::WebAuthnCredentialsDelegate>
      webauthn_delegate_;

  // PasswordCredentialFiller is used to interact with the PasswordManagerDriver
  // to fill the username and password. PasswordCredentialFiller also submits
  // the form if required.
  std::unique_ptr<password_manager::PasswordCredentialFiller> filler_;

  // Shows the password migration warning (expected to be shown after filing
  // user's credentials).
  ShowPasswordMigrationWarningCallback show_password_migration_warning_;

  // Whether the controller should show an option for passkey hybrid sign-in.
  ShowHybridOption should_show_hybrid_option_ = ShowHybridOption(false);

  ukm::SourceId source_id_ = ukm::kInvalidSourceId;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_CONTROLLER_AUTOFILL_DELEGATE_H_
