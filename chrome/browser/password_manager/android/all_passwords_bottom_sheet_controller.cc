// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"
#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view.h"
#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view_impl.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"

using autofill::mojom::FocusedFieldType;
using password_manager::PasswordManagerClient;
using safe_browsing::PasswordReuseDetectionManagerClient;

// No-op constructor for tests.
AllPasswordsBottomSheetController::AllPasswordsBottomSheetController(
    base::PassKey<class AllPasswordsBottomSheetControllerTest>,
    content::WebContents* web_contents,
    std::unique_ptr<AllPasswordsBottomSheetView> view,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    password_manager::PasswordStoreInterface* profile_store,
    password_manager::PasswordStoreInterface* account_store,
    base::OnceCallback<void()> dismissal_callback,
    FocusedFieldType focused_field_type,
    PasswordManagerClient* client,
    PasswordReuseDetectionManagerClient*
        password_reuse_detection_manager_client,
    ShowMigrationWarningCallback show_migration_warning_callback,
    std::unique_ptr<PasswordAccessLossWarningBridge> access_loss_warning_bridge)
    : view_(std::move(view)),
      web_contents_(web_contents),
      profile_store_(profile_store),
      account_store_(account_store),
      dismissal_callback_(std::move(dismissal_callback)),
      driver_(std::move(driver)),
      focused_field_type_(focused_field_type),
      client_(client),
      password_reuse_detection_manager_client_(
          password_reuse_detection_manager_client),
      show_migration_warning_callback_(
          std::move(show_migration_warning_callback)),
      access_loss_warning_bridge_(std::move(access_loss_warning_bridge)),
      plus_address_service_(PlusAddressServiceFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext())) {}

AllPasswordsBottomSheetController::AllPasswordsBottomSheetController(
    content::WebContents* web_contents,
    password_manager::PasswordStoreInterface* profile_store,
    password_manager::PasswordStoreInterface* account_store,
    base::OnceCallback<void()> dismissal_callback,
    FocusedFieldType focused_field_type)
    : view_(std::make_unique<AllPasswordsBottomSheetViewImpl>(this)),
      web_contents_(web_contents),
      profile_store_(profile_store),
      account_store_(account_store),
      dismissal_callback_(std::move(dismissal_callback)),
      focused_field_type_(focused_field_type),
      show_migration_warning_callback_(
          base::BindRepeating(&local_password_migration::ShowWarning)),
      access_loss_warning_bridge_(
          std::make_unique<PasswordAccessLossWarningBridgeImpl>()),
      plus_address_service_(PlusAddressServiceFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext())) {
  CHECK(web_contents_);
  CHECK(profile_store);
  CHECK(dismissal_callback_);
  auto* focused_frame = web_contents->GetFocusedFrame();
  CHECK(focused_frame->IsRenderFrameLive());
  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          focused_frame);
  driver_ = driver->AsWeakPtr();
  client_ = ChromePasswordManagerClient::FromWebContents(web_contents_);
  password_reuse_detection_manager_client_ =
      ChromePasswordReuseDetectionManagerClient::FromWebContents(web_contents_);
}

AllPasswordsBottomSheetController::~AllPasswordsBottomSheetController() {
  if (authenticator_) {
    authenticator_->Cancel();
  }
}

void AllPasswordsBottomSheetController::Show() {
  if (on_password_forms_received_barrier_callback_) {
    return;
  }

  int awaiting_calls = account_store_ ? 2 : 1;
  on_password_forms_received_barrier_callback_ = base::BarrierCallback<
      std::vector<std::unique_ptr<password_manager::PasswordForm>>>(
      awaiting_calls,
      base::BindOnce(
          &AllPasswordsBottomSheetController::OnResultFromAllStoresReceived,
          weak_ptr_factory_.GetWeakPtr()));

  profile_store_->GetAllLoginsWithAffiliationAndBrandingInformation(
      weak_ptr_factory_.GetWeakPtr());
  if (account_store_) {
    account_store_->GetAllLoginsWithAffiliationAndBrandingInformation(
        weak_ptr_factory_.GetWeakPtr());
  }
}

void AllPasswordsBottomSheetController::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  CHECK(on_password_forms_received_barrier_callback_);
  std::erase_if(results,
                [](const auto& form_ptr) { return form_ptr->blocked_by_user; });
  on_password_forms_received_barrier_callback_.Run(std::move(results));
}

Profile* AllPasswordsBottomSheetController::GetProfile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

gfx::NativeView AllPasswordsBottomSheetController::GetNativeView() {
  return web_contents_->GetNativeView();
}

void AllPasswordsBottomSheetController::OnCredentialSelected(
    const std::u16string username,
    const std::u16string password,
    RequestsToFillPassword requests_to_fill_password) {
  const bool is_password_field =
      focused_field_type_ == FocusedFieldType::kFillablePasswordField;
  if (!driver_) {
    OnDismiss();
    return;
  }

  if (requests_to_fill_password && is_password_field) {
    // `client_` is guaranteed to be valid here.
    // Both the `client_` and `PasswordAccessoryController` are attached to
    // WebContents. And AllPasswordBottomSheetController is owned by
    // PasswordAccessoryController.
    DCHECK(client_);
    std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
        client_->GetDeviceAuthenticator();
    if (client_->IsReauthBeforeFillingRequired(authenticator.get())) {
      authenticator_ = std::move(authenticator);
      authenticator_->AuthenticateWithMessage(
          u"",
          base::BindOnce(&AllPasswordsBottomSheetController::OnReauthCompleted,
                         base::Unretained(this), password));
      return;
    }

    FillPassword(password);
  } else if (!requests_to_fill_password) {
    driver_->FillIntoFocusedField(is_password_field, username);
  }
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsMigrationWarning)) {
    show_migration_warning_callback_.Run(
        web_contents_->GetTopLevelNativeWindow(),
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
        password_manager::metrics_util::PasswordMigrationWarningTriggers::
            kAllPasswords);
  }

  TryToShowAccessLossWarningSheet();

  // Consumes the dismissal callback to destroy the native controller and java
  // controller after the user selects a credential.
  OnDismiss();
}

void AllPasswordsBottomSheetController::OnDismiss() {
  std::move(dismissal_callback_).Run();
}

const GURL& AllPasswordsBottomSheetController::GetFrameUrl() {
  return driver_->GetLastCommittedURL();
}

bool AllPasswordsBottomSheetController::IsPlusAddress(
    const std::string& potential_plus_address) const {
  if (!base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled)) {
    return false;
  }
  return plus_address_service_ &&
         plus_address_service_->IsPlusAddress(potential_plus_address);
}

void AllPasswordsBottomSheetController::OnReauthCompleted(
    const std::u16string& password,
    bool auth_succeeded) {
  authenticator_.reset();

  if (auth_succeeded) {
    FillPassword(password);
    TryToShowAccessLossWarningSheet();
  }

  // Consumes the dismissal callback to destroy the native controller and java
  // controller after the user selects a credential.
  OnDismiss();
}

void AllPasswordsBottomSheetController::FillPassword(
    const std::u16string& password) {
  if (!driver_) {
    return;
  }
  driver_->FillIntoFocusedField(true, password);
  password_reuse_detection_manager_client_->OnPasswordSelected(password);
}

void AllPasswordsBottomSheetController::OnResultFromAllStoresReceived(
    std::vector<std::vector<std::unique_ptr<password_manager::PasswordForm>>>
        results) {
  CHECK(on_password_forms_received_barrier_callback_);
  CHECK(!results.empty());
  on_password_forms_received_barrier_callback_.Reset();

  if (results.size() > 1) {
    std::move(results[1].begin(), results[1].end(),
              std::back_inserter(results[0]));
  }
  view_->Show(std::move(results[0]), focused_field_type_);
}

void AllPasswordsBottomSheetController::TryToShowAccessLossWarningSheet() {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning)) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (profile && access_loss_warning_bridge_->ShouldShowAccessLossNoticeSheet(
                     profile->GetPrefs(), /*called_at_startup=*/false)) {
    access_loss_warning_bridge_->MaybeShowAccessLossNoticeSheet(
        profile->GetPrefs(), web_contents_->GetTopLevelNativeWindow(), profile,
        /*called_at_startup=*/false,
        password_manager_android_util::PasswordAccessLossWarningTriggers::
            kAllPasswords);
  }
}
