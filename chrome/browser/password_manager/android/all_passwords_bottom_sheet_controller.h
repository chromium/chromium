// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace password_manager {
class PasswordManagerClient;
class PasswordManagerDriver;
}  // namespace password_manager

namespace safe_browsing {
class PasswordReuseDetectionManagerClient;
}

namespace content {
class WebContents;
}  // namespace content

class AllPasswordsBottomSheetView;

// This class gets credentials and creates AllPasswordsBottomSheetView.
class AllPasswordsBottomSheetController
    : public password_manager::PasswordStoreConsumer {
 public:
  using RequestsToFillPassword =
      base::StrongAlias<struct RequestsToFillPasswordTag, bool>;
  // No-op constructor for tests.
  AllPasswordsBottomSheetController(
      base::PassKey<class AllPasswordsBottomSheetControllerTest>,
      std::unique_ptr<AllPasswordsBottomSheetView> view,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      password_manager::PasswordStoreInterface* store,
      base::OnceCallback<void()> dismissal_callback,
      autofill::mojom::FocusedFieldType focused_field_type,
      password_manager::PasswordManagerClient* client,
      safe_browsing::PasswordReuseDetectionManagerClient*
          password_reuse_detection_manager_client);

  AllPasswordsBottomSheetController(
      content::WebContents* web_contents,
      password_manager::PasswordStoreInterface* store,
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

  // The web page view containing the focused field.
  gfx::NativeView GetNativeView();

  // Called from the view when the user dismisses the BottomSheet
  // consumes |dismissal_callback|.
  void OnDismiss();

  // Returns the last committed URL of the frame from |driver_|.
  const GURL& GetFrameUrl();

 private:
  // Called when the biometric re-auth completes. |password| is the password
  // to be filled and |auth_succeded| is the authentication result.
  void OnReauthCompleted(const std::u16string& password, bool auth_succeeded);

  // Fills the password into the focused field.
  void FillPassword(const std::u16string& password);

  // The controller takes |view_| ownership.
  std::unique_ptr<AllPasswordsBottomSheetView> view_;

  // This controller doesn't take |web_contents_| ownership.
  // This controller is attached to this |web_contents_| lifetime. It will be
  // destroyed if |web_contents_| is destroyed.
  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // The controller doesn't take |store_| ownership.
  raw_ptr<password_manager::PasswordStoreInterface> store_;

  // A callback method will be consumed when the user dismisses the BottomSheet.
  base::OnceCallback<void()> dismissal_callback_;

  // Either |driver_| is created and owned by this controller or received in
  // constructor specified for tests.
  base::WeakPtr<password_manager::PasswordManagerDriver> driver_;

  // Authenticator used to trigger a biometric re-auth before password filling.
  scoped_refptr<device_reauth::DeviceAuthenticator> authenticator_;

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

  base::WeakPtrFactory<AllPasswordsBottomSheetController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_
