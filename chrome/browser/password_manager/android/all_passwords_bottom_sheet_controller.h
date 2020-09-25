// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_

#include "base/callback.h"
#include "base/util/type_safety/pass_key.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace password_manager {
class PasswordManagerDriver;
}  // namespace password_manager

namespace content {
class WebContents;
}  // namespace content

class AllPasswordsBottomSheetView;

// This class gets credentials and creates AllPasswordsBottomSheetView.
class AllPasswordsBottomSheetController
    : public password_manager::PasswordStoreConsumer {
 public:
  // No-op constructor for tests.
  AllPasswordsBottomSheetController(
      util::PassKey<class AllPasswordsBottomSheetControllerTest>,
      std::unique_ptr<AllPasswordsBottomSheetView> view,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      password_manager::PasswordStore* store,
      base::OnceCallback<void()> dismissal_callback,
      autofill::mojom::FocusedFieldType focused_field_type);

  AllPasswordsBottomSheetController(
      content::WebContents* web_contents,
      password_manager::PasswordStore* store,
      base::OnceCallback<void()> dismissal_callback,
      autofill::mojom::FocusedFieldType focused_field_type);
  ~AllPasswordsBottomSheetController() override;
  AllPasswordsBottomSheetController(const AllPasswordsBottomSheetController&) =
      delete;
  AllPasswordsBottomSheetController& operator=(
      const AllPasswordsBottomSheetController&) = delete;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // Instructs AllPasswordsBottomSheetView to show the credentials to the user.
  void Show();

  // Informs the controller that the user has made a selection.
  void OnCredentialSelected(const base::string16 username,
                            const base::string16 password);

  // The web page view containing the focused field.
  gfx::NativeView GetNativeView();

  // Called from the view when the user dismisses the BottomSheet
  // consumes |dismissal_callback|.
  void OnDismiss();

  // Returns the last committed URL of the frame from |driver_|.
  const GURL& GetFrameUrl();

 private:
  // The controller takes |view_| ownership.
  std::unique_ptr<AllPasswordsBottomSheetView> view_;

  // This controller doesn't take |web_contents_| ownership.
  // This controller is attached to this |web_contents_| lifetime. It will be
  // destroyed if |web_contents_| is destroyed.
  content::WebContents* web_contents_ = nullptr;

  // The controller doesn't take |store_| ownership.
  password_manager::PasswordStore* store_;

  // A callback method will be consumed when the user dismisses the BottomSheet.
  base::OnceCallback<void()> dismissal_callback_;

  // Either |driver_| is created and owned by this controller or received in
  // constructor specified for tests.
  base::WeakPtr<password_manager::PasswordManagerDriver> driver_;

  // The type of field on which the user is focused, e.g. PASSWORD.
  autofill::mojom::FocusedFieldType focused_field_type_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ALL_PASSWORDS_BOTTOM_SHEET_CONTROLLER_H_
