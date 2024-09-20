// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_

#include "base/barrier_callback.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace password_manager {
class PasswordManagerClient;
class PasswordManagerDriver;
}  // namespace password_manager

namespace plus_addresses {
class PlusAddressService;
}  // namespace plus_addresses

namespace safe_browsing {
class PasswordReuseDetectionManagerClient;
}

namespace content {
class WebContents;
}  // namespace content

class AllPasswordsBottomSheetView;
class Profile;

// This class gets credentials and creates AllPasswordsBottomSheetView.
class AllPasswordsBottomSheetController
    : public password_manager::PasswordStoreConsumer {
 public:
  using RequestsToFillPassword =
      base::StrongAlias<struct RequestsToFillPasswordTag, bool>;
  using ShowMigrationWarningCallback = base::RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>;
  // No-op constructor for tests.
  AllPasswordsBottomSheetController(
      base::PassKey<class AllPasswordsBottomSheetControllerTest>,
      content::WebContents* web_contents,
      std::unique_ptr<AllPasswordsBottomSheetView> view,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      password_manager::PasswordStoreInterface* profile_store,
      password_manager::PasswordStoreInterface* account_store,
      base::OnceCallback<void()> dismissal_callback,
      autofill::mojom::FocusedFieldType focused_field_type,
      password_manager::PasswordManagerClient* client,
      safe_browsing::PasswordReuseDetectionManagerClient*
          password_reuse_detection_manager_client,
      ShowMigrationWarningCallback show_migration_warning_callback,
      std::unique_ptr<PasswordAccessLossWarningBridge>
          access_loss_warning_bridge);

  AllPasswordsBottomSheetController(
      content::WebContents* web_contents,
      password_manager::PasswordStoreInterface* profile_store,
      password_manager::PasswordStoreInterface* account_store,
      base::OnceCallback<void()> dismissal_callback,
      autofill::mojom::FocusedFieldType focused_field_type);
  ~AllPasswordsBottomSheetController() override;
  AllPasswordsBottomSheetController(const AllPasswordsBottomSheetController&) =
      delete;
  AllPasswordsBottomSheetController& operator=(
      const AllPasswordsBottomSheetController&) = delete;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  // Instructs AllPasswordsBottomSheetView to show the credentials to the user.
  void Show();

  // Informs the controller that the user has made a selection.
  void OnCredentialSelected(const std::u16string username,
                            const std::u16string password,
                            RequestsToFillPassword requests_to_fill_password);

  // The Profile associated with the displayed web contents.
  Profile* GetProfile();

  // The web page view containing the focused field.
  gfx::NativeView GetNativeView();

  // Called from the view when the user dismisses the BottomSheet
  // consumes |dismissal_callback|.
  void OnDismiss();

  // Returns the last committed URL of the frame from |driver_|.
  const GURL& GetFrameUrl();

  // Uses `PlusAddressService` as a source of truth to check if the
  // `maybe_plus_address` is an existing plus address.
  bool IsPlusAddress(const std::string& potential_plus_address) const;

 private:
  // Called when the biometric re-auth completes. |password| is the password
  // to be filled and |auth_succeded| is the authentication result.
  void OnReauthCompleted(const std::u16string& password, bool auth_succeeded);

  // Fills the password into the focused field.
  void FillPassword(const std::u16string& password);

  void OnResultFromAllStoresReceived(
      std::vector<std::vector<std::unique_ptr<password_manager::PasswordForm>>>
          results);

  // Shows the access loss warning sheet if needed. It's used after filling a
  // credential.
  void TryToShowAccessLossWarningSheet();

  // The controller takes |view_| ownership.
  std::unique_ptr<AllPasswordsBottomSheetView> view_;

  // This controller doesn't take |web_contents_| ownership.
  // This controller is attached to this |web_contents_| lifetime. It will be
  // destroyed if |web_contents_| is destroyed.
  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // The controller doesn't take |store_| ownership.
  raw_ptr<password_manager::PasswordStoreInterface> profile_store_;
  raw_ptr<password_manager::PasswordStoreInterface> account_store_;

  // Allows to aggregate GetAllLogins results from multiple stores.
  base::RepeatingCallback<void(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>)>
      on_password_forms_received_barrier_callback_;

  // A callback method will be consumed when the user dismisses the BottomSheet.
  base::OnceCallback<void()> dismissal_callback_;

  // Either |driver_| is created and owned by this controller or received in
  // constructor specified for tests.
  base::WeakPtr<password_manager::PasswordManagerDriver> driver_;

  // Authenticator used to trigger a biometric re-auth before password filling.
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator_;

  // The type of field on which the user is focused, e.g. PASSWORD.
  autofill::mojom::FocusedFieldType focused_field_type_;

  // The PasswordManagerClient associated with the current |web_contents_|.
  // Used to get a pointer to a BiometricAuthenticator.
  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  // The passwordReuseDetectionManagerClient associated with the current
  // |web_contents_|. Used to tell `PasswordReuseDetectionManager` that a
  // password has been reused.
  raw_ptr<safe_browsing::PasswordReuseDetectionManagerClient>
      password_reuse_detection_manager_client_ = nullptr;

  // Callback invoked to try to show the password migration warning. Used
  // to facilitate testing.
  ShowMigrationWarningCallback show_migration_warning_callback_;

  // Bridge that is used to show the password access loss warning if it's needed
  // after filling a credential.
  std::unique_ptr<PasswordAccessLossWarningBridge> access_loss_warning_bridge_;

  // `PlusAddressService` is used to check which credentials have a plus address
  // as a username.
  raw_ptr<const plus_addresses::PlusAddressService> plus_address_service_;

  base::WeakPtrFactory<AllPasswordsBottomSheetController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_
