// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view_factory.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/native_widget_types.h"

namespace password_manager {
class PasswordManagerClient;
class PasswordManagerDriver;
class UiCredential;
}  // namespace password_manager

class ChromePasswordManagerClient;
class TouchToFillWebAuthnCredential;

class TouchToFillController {
 public:
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
    kSelectedWebAuthnCredential = 3,
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
    kWebAuthnCredentialSelected = 4,
    kMaxValue = kWebAuthnCredentialSelected,
  };

  // No-op constructor for tests.
  TouchToFillController(
      base::PassKey<class TouchToFillControllerTest>,
      password_manager::PasswordManagerClient* password_client,
      scoped_refptr<device_reauth::BiometricAuthenticator> authenticator);

  TouchToFillController(
      ChromePasswordManagerClient* password_client,
      scoped_refptr<device_reauth::BiometricAuthenticator> authenticator);
  TouchToFillController(const TouchToFillController&) = delete;
  TouchToFillController& operator=(const TouchToFillController&) = delete;
  ~TouchToFillController();

  // Instructs the controller to show the provided |credentials| and
  // |webauthn_credentials| to the user.
  void Show(base::span<const password_manager::UiCredential> credentials,
            base::span<TouchToFillWebAuthnCredential> webauthn_credentials,
            base::WeakPtr<password_manager::PasswordManagerDriver> driver,
            autofill::mojom::SubmissionReadinessState submission_readiness);

  // Informs the controller that the user has made a selection. Invokes both
  // FillSuggestion() and TouchToFillDismissed() on |driver_|. No-op if invoked
  // repeatedly.
  void OnCredentialSelected(const password_manager::UiCredential& credential);

  // Informs the controller that the user has made a selection. Invokes
  // TouchToFillDismissed() and initiates a WebAuthn sign-in.
  void OnWebAuthnCredentialSelected(
      const TouchToFillWebAuthnCredential& credential);

  // Informs the controller that the user has tapped the "Manage Passwords"
  // button. This will open the password preferences.
  void OnManagePasswordsSelected();

  // Informs the controller that the user has dismissed the sheet. Invokes
  // TouchToFillDismissed() on |driver_|. No-op if invoked repeatedly.
  void OnDismiss();

  // The web page view containing the focused field.
  gfx::NativeView GetNativeView();

#if defined(UNIT_TEST)
  void set_view(std::unique_ptr<TouchToFillView> view) {
    view_ = std::move(view);
  }
#endif

 private:
  // Called after the biometric reauth completes. If `authSuccessful` is
  // true, `credential` will be filled into the form.
  void OnReauthCompleted(password_manager::UiCredential credential,
                         bool authSuccessful);

  // Fills the credential into the form.
  void FillCredential(const password_manager::UiCredential& credential);

  // Called upon completion or dismissal to perform cleanup.
  void CleanUpDriverAndReportOutcome(TouchToFillOutcome outcome,
                                     bool show_virtual_keyboard);

  // Weak pointer to the PasswordManagerClient this class is tied to.
  raw_ptr<password_manager::PasswordManagerClient> password_client_ = nullptr;

  // Driver passed to the latest invocation of Show(). Gets cleared when
  // OnCredentialSelected() or OnDismissed() gets called.
  base::WeakPtr<password_manager::PasswordManagerDriver> driver_;

  // Whether the controller should trigger submission when a credential is
  // filled in.
  bool trigger_submission_ = false;

  // Whether a form is ready for submission. Similar to |trigger_submission_|,
  // but doesn't depend on flags. Used for dark launch metrics (e.g. time
  // between filling and successful login with and without
  // kTouchToFillPasswordSubmission enabled). TODO(crbug.com/1299394): remove
  // after the launch.
  bool ready_for_submission_ = false;

  // Authenticator used to trigger a biometric auth before filling.
  scoped_refptr<device_reauth::BiometricAuthenticator> authenticator_;

  ukm::SourceId source_id_ = ukm::kInvalidSourceId;

  // View used to communicate with the Android frontend. Lazily instantiated so
  // that it can be injected by tests.
  std::unique_ptr<TouchToFillView> view_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_CONTROLLER_H_
