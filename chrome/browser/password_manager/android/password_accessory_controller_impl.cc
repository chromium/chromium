// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_accessory_controller_impl.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_utils.h"
#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"
#include "chrome/browser/password_manager/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_accessory_metrics_util.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::AccessorySheetData;
using autofill::AccessorySheetField;
using autofill::FooterCommand;
using autofill::UserInfo;
using autofill::mojom::FocusedFieldType;
using password_manager::CredentialCache;
using password_manager::PasswordStoreInterface;
using password_manager::UiCredential;
using BlocklistedStatus =
    password_manager::OriginCredentialStore::BlocklistedStatus;
using FillingSource = ManualFillingController::FillingSource;
using IsExactMatch = autofill::UserInfo::IsExactMatch;
using ShouldShowAction = ManualFillingController::ShouldShowAction;

namespace {

autofill::UserInfo TranslateCredentials(bool current_field_is_password,
                                        const url::Origin& frame_origin,
                                        const UiCredential& credential) {
  DCHECK(!credential.origin().opaque());
  UserInfo user_info(
      credential.origin().Serialize(),
      IsExactMatch(credential.match_type() ==
                   password_manager_util::GetLoginMatchType::kExact));

  std::u16string username = GetDisplayUsername(credential);
  user_info.add_field(AccessorySheetField(
      /*display_text=*/username, /*text_to_fill=*/username,
      /*a11y_description=*/username, /*id=*/std::string(),
      /*is_obfuscated=*/false,
      /*selectable=*/!credential.username().empty()));

  user_info.add_field(AccessorySheetField(
      /*display_text=*/credential.password(),
      /*text_to_fill=*/credential.password(),
      /*a11y_description=*/
      l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, username),
      /*id=*/std::string(),
      /*is_obfuscated=*/true, /*selectable=*/current_field_is_password));

  return user_info;
}

std::u16string GetTitle(bool has_suggestions, const url::Origin& origin) {
  const std::u16string elided_url =
      url_formatter::FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  return l10n_util::GetStringFUTF16(
      has_suggestions
          ? IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE
          : IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE,
      elided_url);
}

password_manager::PasswordManagerDriver* GetPasswordManagerDriver(
    content::WebContents* web_contents) {
  password_manager::ContentPasswordManagerDriverFactory* factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents);
  return factory->GetDriverForFrame(web_contents->GetFocusedFrame());
}

ShouldShowAction ShouldShowCredManReentryAction(
    autofill::mojom::FocusedFieldType focused_field_type,
    bool has_pending_credman_flow) {
  if (!has_pending_credman_flow) {
    return ShouldShowAction(false);
  }
  switch (focused_field_type) {
    case autofill::mojom::FocusedFieldType::kFillablePasswordField:
    case autofill::mojom::FocusedFieldType::kFillableUsernameField:
    case autofill::mojom::FocusedFieldType::kFillableWebauthnTaggedField:
      return ShouldShowAction(true);
    case autofill::mojom::FocusedFieldType::kFillableNonSearchField:
    case autofill::mojom::FocusedFieldType::kFillableSearchField:
    case autofill::mojom::FocusedFieldType::kFillableTextArea:
    case autofill::mojom::FocusedFieldType::kUnfillableElement:
    case autofill::mojom::FocusedFieldType::kUnknown:
      return ShouldShowAction(false);
  }
  NOTREACHED_NORETURN() << "Showing undefined for " << focused_field_type;
}

}  // namespace

PasswordAccessoryControllerImpl::~PasswordAccessoryControllerImpl() {
  if (authenticator_) {
    authenticator_->Cancel(device_reauth::DeviceAuthRequester::kFallbackSheet);
  }
}

void PasswordAccessoryControllerImpl::RegisterFillingSourceObserver(
    FillingSourceObserver observer) {
  source_observer_ = std::move(observer);
}

absl::optional<autofill::AccessorySheetData>
PasswordAccessoryControllerImpl::GetSheetData() const {
  // Prevent crashing by returning a nullopt if no field was focused yet or if
  // the frame was (possibly temporarily) unfocused. This signals to the caller
  // that no sheet is available right now.
  if (GetWebContents().GetFocusedFrame() == nullptr)
    return absl::nullopt;
  if (!last_focused_field_info_)
    return absl::nullopt;
  url::Origin origin = GetFocusedFrameOrigin();
  // If the focused origin doesn't match the last known origin, it is not safe
  // to provide any suggestions (because e.g. information about field type isn't
  // reliable).
  if (!last_focused_field_info_->origin.IsSameOriginWith(origin))
    return absl::nullopt;

  std::vector<UserInfo> info_to_add;
  std::vector<FooterCommand> footer_commands_to_add;
  const bool is_password_field = last_focused_field_info_->focused_field_type ==
                                 FocusedFieldType::kFillablePasswordField;

  if (autofill::IsFillable(last_focused_field_info_->focused_field_type)) {
    base::span<const UiCredential> suggestions =
        credential_cache_->GetCredentialStore(origin).GetCredentials();
    info_to_add.reserve(suggestions.size());
    for (const auto& credential : suggestions) {
      if (credential.match_type() ==
              password_manager_util::GetLoginMatchType::kPSL &&
          !base::FeatureList::IsEnabled(
              autofill::features::kAutofillKeyboardAccessory)) {
        continue;  // PSL origins have no representation in V1. Don't show them!
      }
      info_to_add.push_back(
          TranslateCredentials(is_password_field, origin, credential));
    }
  }

  if (all_passwords_helper_.available_credentials().has_value() &&
      IsSecureSite() && origin.GetURL().SchemeIsCryptographic() &&
      all_passwords_helper_.available_credentials().value() > 0) {
    footer_commands_to_add.push_back(
        FooterCommand(l10n_util::GetStringUTF16(
                          IDS_PASSWORD_MANAGER_ACCESSORY_SELECT_PASSWORD),
                      autofill::AccessoryAction::USE_OTHER_PASSWORD));
  }

  if (is_password_field &&
      last_focused_field_info_->is_manual_generation_available) {
    std::u16string generate_password_title = l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_ACCESSORY_GENERATE_PASSWORD_BUTTON_TITLE);
    footer_commands_to_add.push_back(
        FooterCommand(generate_password_title,
                      autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL));
  }

  std::u16string manage_passwords_title = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
  footer_commands_to_add.push_back(FooterCommand(
      manage_passwords_title, autofill::AccessoryAction::MANAGE_PASSWORDS));

  if (password_manager::PasswordManagerDriver* driver =
          driver_supplier_.Run((&GetWebContents()))) {
    if (password_manager::WebAuthnCredentialsDelegate* credentials_delegate =
            password_client_->GetWebAuthnCredentialsDelegateForDriver(driver)) {
      if (credentials_delegate->IsAndroidHybridAvailable()) {
        std::u16string passkey_other_device_title = l10n_util::GetStringUTF16(
            IDS_PASSWORD_MANAGER_ACCESSORY_USE_DEVICE_PASSKEY);
        footer_commands_to_add.emplace_back(
            passkey_other_device_title,
            autofill::AccessoryAction::CROSS_DEVICE_PASSKEY);
      }
    }
  }

  bool has_suggestions = !info_to_add.empty();
  AccessorySheetData data = autofill::CreateAccessorySheetData(
      autofill::AccessoryTabType::PASSWORDS, GetTitle(has_suggestions, origin),
      std::move(info_to_add), std::move(footer_commands_to_add));

  if (ShouldShowRecoveryToggle(origin)) {
    BlocklistedStatus blocklisted_status =
        credential_cache_->GetCredentialStore(origin).GetBlocklistedStatus();
    if (blocklisted_status == BlocklistedStatus::kWasBlocklisted ||
        blocklisted_status == BlocklistedStatus::kIsBlocklisted) {
      autofill::OptionToggle option_toggle = autofill::OptionToggle(
          l10n_util::GetStringUTF16(IDS_PASSWORD_SAVING_STATUS_TOGGLE),
          /*enabled=*/blocklisted_status == BlocklistedStatus::kWasBlocklisted,
          autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS);
      data.set_option_toggle(option_toggle);
    }
  }
  return data;
}

void PasswordAccessoryControllerImpl::OnFillingTriggered(
    autofill::FieldGlobalId focused_field_id,
    const AccessorySheetField& selection) {
  if (!ShouldTriggerBiometricReauth(selection)) {
    FillSelection(selection);
    return;
  }

  authenticator_ = password_client_->GetDeviceAuthenticator();

  // |this| cancels the authentication when it is destroyed if one is ongoing,
  // which resets the callback, so it's safe to use base::Unretained(this) here.
  authenticator_->Authenticate(
      device_reauth::DeviceAuthRequester::kFallbackSheet,
      base::BindOnce(&PasswordAccessoryControllerImpl::OnReauthCompleted,
                     base::Unretained(this), selection),
      /*use_last_valid_auth=*/true);
}

// static
PasswordAccessoryController* PasswordAccessoryController::GetOrCreate(
    content::WebContents* web_contents,
    password_manager::CredentialCache* credential_cache) {
  PasswordAccessoryControllerImpl::CreateForWebContents(web_contents,
                                                        credential_cache);
  return PasswordAccessoryControllerImpl::FromWebContents(web_contents);
}

// static
PasswordAccessoryController* PasswordAccessoryController::GetIfExisting(
    content::WebContents* web_contents) {
  return PasswordAccessoryControllerImpl::FromWebContents(web_contents);
}

// static
void PasswordAccessoryControllerImpl::CreateForWebContents(
    content::WebContents* web_contents,
    password_manager::CredentialCache* credential_cache) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(credential_cache);

  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new PasswordAccessoryControllerImpl(
            web_contents, credential_cache, nullptr,
            ChromePasswordManagerClient::FromWebContents(web_contents),
            base::BindRepeating(GetPasswordManagerDriver),
            base::BindRepeating(&local_password_migration::ShowWarning))));
  }
}

// static
void PasswordAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    password_manager::CredentialCache* credential_cache,
    base::WeakPtr<ManualFillingController> manual_filling_controller,
    password_manager::PasswordManagerClient* password_client,
    PasswordDriverSupplierForFocusedFrame driver_supplier,
    ShowMigrationWarningCallback show_migration_warning_callback) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(manual_filling_controller);
  DCHECK(password_client);

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new PasswordAccessoryControllerImpl(
          web_contents, credential_cache, std::move(manual_filling_controller),
          password_client, std::move(driver_supplier),
          std::move(show_migration_warning_callback))));
}

void PasswordAccessoryControllerImpl::OnOptionSelected(
    autofill::AccessoryAction selected_action) {
  switch (selected_action) {
    case autofill::AccessoryAction::USE_OTHER_PASSWORD:
      ShowAllPasswords();
      return;
    case autofill::AccessoryAction::MANAGE_PASSWORDS:
      password_manager_launcher::ShowPasswordSettings(
          &GetWebContents(),
          password_manager::ManagePasswordsReferrer::kPasswordsAccessorySheet,
          /*manage_passkeys=*/false);
      return;
    case autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL:
      OnGenerationRequested(
          autofill::password_generation::PasswordGenerationType::kManual);
      GetManualFillingController()->Hide();
      return;
    case autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC:
      OnGenerationRequested(
          autofill::password_generation::PasswordGenerationType::kAutomatic);
      GetManualFillingController()->Hide();
      return;
    case autofill::AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY:
      if (password_manager::PasswordManagerDriver* driver =
              driver_supplier_.Run(&GetWebContents())) {
        if (webauthn::WebAuthnCredManDelegate* delegate =
                password_client_->GetWebAuthnCredManDelegateForDriver(driver)) {
          delegate->TriggerFullRequest();
        }
      }
      return;
    case autofill::AccessoryAction::CROSS_DEVICE_PASSKEY:
      if (password_manager::PasswordManagerDriver* driver =
              driver_supplier_.Run(&GetWebContents())) {
        if (password_manager::
                WebAuthnCredentialsDelegate* credentials_delegate =
                    password_client_->GetWebAuthnCredentialsDelegateForDriver(
                        driver)) {
          CHECK(credentials_delegate->IsAndroidHybridAvailable());
          credentials_delegate->ShowAndroidHybridSignIn();
        }
      }
      return;
    default:
      NOTREACHED() << "Unhandled selected action: "
                   << static_cast<int>(selected_action);
  }
}

void PasswordAccessoryControllerImpl::OnToggleChanged(
    autofill::AccessoryAction toggled_action,
    bool enabled) {
  if (toggled_action == autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS) {
    ChangeCurrentOriginSavePasswordsStatus(enabled);
    return;
  }
  NOTREACHED() << "Unhandled selected action: "
               << static_cast<int>(toggled_action);
}

void PasswordAccessoryControllerImpl::RefreshSuggestionsForField(
    FocusedFieldType focused_field_type,
    bool is_manual_generation_available) {
  // Discard all frame data. This ensures that the data is never used for an
  // incorrect frame.
  last_focused_field_info_ = absl::nullopt;
  all_passwords_helper_.SetLastFocusedFieldType(focused_field_type);

  // Prevent crashing by not acting at all if frame became unfocused at any
  // point. The next time a focus event happens, this will be called again and
  // ensure we show correct data.
  if (GetWebContents().GetFocusedFrame() == nullptr)
    return;
  url::Origin origin = GetFocusedFrameOrigin();
  if (origin.opaque())
    return;  // Don't proceed for invalid origins.
  TRACE_EVENT0("passwords",
               "PasswordAccessoryControllerImpl::RefreshSuggestionsForField");
  last_focused_field_info_.emplace(origin, focused_field_type,
                                   is_manual_generation_available);
  bool sheet_provides_value = is_manual_generation_available;

  all_passwords_helper_.ClearUpdateCallback();
  if (!all_passwords_helper_.available_credentials().has_value()) {
    all_passwords_helper_.SetUpdateCallback(base::BindOnce(
        &PasswordAccessoryControllerImpl::RefreshSuggestionsForField,
        base::Unretained(this), focused_field_type,
        is_manual_generation_available));
  } else {
    sheet_provides_value |=
        all_passwords_helper_.available_credentials().value() > 0;
  }

  if (ShouldShowRecoveryToggle(origin)) {
    if (credential_cache_->GetCredentialStore(origin).GetBlocklistedStatus() ==
        BlocklistedStatus::kIsBlocklisted) {
      UMA_HISTOGRAM_BOOLEAN(
          "KeyboardAccessory.DisabledSavingAccessoryImpressions", true);
    }
    sheet_provides_value = true;
  }

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory)) {
    DCHECK(source_observer_);
    // The all passwords sheet could cover this but if it's still loading, use
    // this data as the next closest proxy to minimize delayed updates UI.
    sheet_provides_value |=
        !credential_cache_->GetCredentialStore(origin).GetCredentials().empty();
    // The "Manage Passwords" entry point doesn't justify showing this fallback
    // sheet for non-password fields.
    source_observer_.Run(this, IsFillingSourceAvailable(
                                   autofill::IsFillable(focused_field_type) &&
                                   sheet_provides_value));
  } else {
    absl::optional<AccessorySheetData> data = GetSheetData();
    DCHECK(data.has_value());
    GetManualFillingController()->RefreshSuggestions(std::move(data.value()));
  }
}

void PasswordAccessoryControllerImpl::OnGenerationRequested(
    autofill::password_generation::PasswordGenerationType type) {
  PasswordGenerationController* pwd_generation_controller =
      PasswordGenerationController::GetIfExisting(&GetWebContents());

  DCHECK(pwd_generation_controller);
  pwd_generation_controller->OnGenerationRequested(type);
}

void PasswordAccessoryControllerImpl::UpdateCredManReentryUi(
    autofill::mojom::FocusedFieldType focused_field_type) {
  if (!webauthn::WebAuthnCredManDelegate::IsCredManEnabled()) {
    return;  // No updates required.
  }
  if (password_manager::PasswordManagerDriver* driver =
          driver_supplier_.Run(&GetWebContents())) {
    if (webauthn::WebAuthnCredManDelegate* delegate =
            password_client_->GetWebAuthnCredManDelegateForDriver(driver)) {
      GetManualFillingController()->OnAccessoryActionAvailabilityChanged(
          ShouldShowCredManReentryAction(focused_field_type,
                                         delegate->HasResults()),
          autofill::AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY);
    }
  }
}

PasswordAccessoryControllerImpl::LastFocusedFieldInfo::LastFocusedFieldInfo(
    url::Origin focused_origin,
    autofill::mojom::FocusedFieldType focused_field,
    bool manual_generation_available)
    : origin(focused_origin),
      focused_field_type(focused_field),
      is_manual_generation_available(manual_generation_available) {}

PasswordAccessoryControllerImpl::PasswordAccessoryControllerImpl(
    content::WebContents* web_contents,
    password_manager::CredentialCache* credential_cache,
    base::WeakPtr<ManualFillingController> manual_filling_controller,
    password_manager::PasswordManagerClient* password_client,
    PasswordDriverSupplierForFocusedFrame driver_supplier,
    ShowMigrationWarningCallback show_migration_warning_callback)
    : content::WebContentsUserData<PasswordAccessoryControllerImpl>(
          *web_contents),
      credential_cache_(credential_cache),
      manual_filling_controller_(std::move(manual_filling_controller)),
      password_client_(password_client),
      driver_supplier_(std::move(driver_supplier)),
      show_migration_warning_callback_(
          std::move(show_migration_warning_callback)) {}

void PasswordAccessoryControllerImpl::ChangeCurrentOriginSavePasswordsStatus(
    bool saving_enabled) {
  const url::Origin origin = GetFocusedFrameOrigin();
  if (origin.opaque())
    return;

  const GURL origin_as_gurl = origin.GetURL();
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml,
      password_manager::GetSignonRealm(origin_as_gurl), origin_as_gurl);
  password_manager::PasswordStoreInterface* store =
      password_client_->GetProfilePasswordStore();
  if (saving_enabled) {
    store->Unblocklist(form_digest);
  } else {
    password_manager::PasswordForm form =
        password_manager_util::MakeNormalizedBlocklistedForm(
            std::move(form_digest));
    form.date_created = base::Time::Now();
    store->AddLogin(form);
  }
  password_client_->UpdateFormManagers();
}

bool PasswordAccessoryControllerImpl::AppearsInSuggestions(
    const std::u16string& suggestion,
    bool is_password,
    const url::Origin& origin) const {
  if (origin.opaque())
    return false;  // Don't proceed for invalid origins.

  return base::ranges::any_of(
      credential_cache_->GetCredentialStore(origin).GetCredentials(),
      [&](const auto& cred) {
        return suggestion == (is_password ? cred.password() : cred.username());
      });
}

bool PasswordAccessoryControllerImpl::ShouldShowRecoveryToggle(
    const url::Origin& origin) const {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kRecoverFromNeverSaveAndroid)) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory)) {
    return false;
  }
  return password_client_->IsSavingAndFillingEnabled(origin.GetURL());
}

base::WeakPtr<ManualFillingController>
PasswordAccessoryControllerImpl::GetManualFillingController() {
  if (!manual_filling_controller_) {
    manual_filling_controller_ =
        ManualFillingController::GetOrCreate(&GetWebContents());
  }
  DCHECK(manual_filling_controller_);
  return manual_filling_controller_;
}

url::Origin PasswordAccessoryControllerImpl::GetFocusedFrameOrigin() const {
  if (GetWebContents().GetFocusedFrame() == nullptr) {
    LOG(DFATAL) << "Tried to get retrieve origin without focused "
                   "frame.";
    return url::Origin();  // Nonce!
  }
  return GetWebContents().GetFocusedFrame()->GetLastCommittedOrigin();
}

void PasswordAccessoryControllerImpl::ShowAllPasswords() {
  // If the controller is initialized that means that the UI is showing.
  if (all_passords_bottom_sheet_controller_ || !last_focused_field_info_) {
    return;
  }

  // AllPasswordsBottomSheetController assumes that the focused frame has a live
  // RenderFrame so that it can use the password manager driver.
  // TODO(https://crbug.com/1286779): Investigate if focused frame really needs
  // to return RenderFrameHosts with non-live RenderFrames.
  if (!GetWebContents().GetFocusedFrame()->IsRenderFrameLive())
    return;

  // We can use |base::Unretained| safely because at the time of calling
  // |AllPasswordsSheetDismissed| we are sure that this controller is alive as
  // it owns |AllPasswordsBottomSheetController| from which the method is
  // called.
  // TODO(crbug.com/1104132): Update the controller with the last focused field.
  all_passords_bottom_sheet_controller_ =
      std::make_unique<AllPasswordsBottomSheetController>(
          &GetWebContents(), password_client_->GetProfilePasswordStore(),
          base::BindOnce(
              &PasswordAccessoryControllerImpl::AllPasswordsSheetDismissed,
              base::Unretained(this)),
          last_focused_field_info_->focused_field_type);

  all_passords_bottom_sheet_controller_->Show();
}

bool PasswordAccessoryControllerImpl::ShouldTriggerBiometricReauth(
    const AccessorySheetField& selection) const {
  if (!selection.is_obfuscated())
    return false;

  scoped_refptr<device_reauth::DeviceAuthenticator> authenticator =
      password_client_->GetDeviceAuthenticator();
  return password_manager_util::CanUseBiometricAuth(authenticator.get(),
                                                    password_client_);
}

void PasswordAccessoryControllerImpl::OnReauthCompleted(
    AccessorySheetField selection,
    bool auth_succeeded) {
  authenticator_.reset();
  if (!auth_succeeded)
    return;
  FillSelection(selection);
}

void PasswordAccessoryControllerImpl::FillSelection(
    const AccessorySheetField& selection) {
  if (!AppearsInSuggestions(selection.display_text(), selection.is_obfuscated(),
                            GetFocusedFrameOrigin())) {
    NOTREACHED() << "Tried to fill '" << selection.display_text() << "' into "
                 << GetFocusedFrameOrigin();
    return;  // Never fill across different origins!
  }
  password_manager::PasswordManagerDriver* driver =
      driver_supplier_.Run(&GetWebContents());
  if (!driver)
    return;
  driver->FillIntoFocusedField(selection.is_obfuscated(),
                               selection.display_text());
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsMigrationWarning)) {
    show_migration_warning_callback_.Run(
        GetWebContents().GetTopLevelNativeWindow(),
        Profile::FromBrowserContext(GetWebContents().GetBrowserContext()),
        password_manager::metrics_util::PasswordMigrationWarningTriggers::
            kKeyboardAcessorySheet);
  }
}

void PasswordAccessoryControllerImpl::AllPasswordsSheetDismissed() {
  all_passords_bottom_sheet_controller_.reset();
}

bool PasswordAccessoryControllerImpl::IsSecureSite() const {
  if (security_level_for_testing_) {
    return security_level_for_testing_ == security_state::SECURE;
  }

  SecurityStateTabHelper::CreateForWebContents(&GetWebContents());
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(&GetWebContents());

  return helper && helper->GetSecurityLevel() == security_state::SECURE;
}

content::WebContents& PasswordAccessoryControllerImpl::GetWebContents() const {
  // While a const_cast is not ideal. The Autofill API uses const in various
  // spots and the content public API doesn't have const accessors. So the const
  // cast is the lesser of two evils.
  return const_cast<content::WebContents&>(
      content::WebContentsUserData<
          PasswordAccessoryControllerImpl>::GetWebContents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PasswordAccessoryControllerImpl);
