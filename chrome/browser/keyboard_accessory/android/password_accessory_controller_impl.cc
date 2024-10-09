// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/password_accessory_controller_impl.h"

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_utils.h"
#include "chrome/browser/keyboard_accessory/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"
#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/android/plus_addresses/all_plus_addresses_bottom_sheet_controller.h"
#include "chrome/browser/ui/android/plus_addresses/plus_addresses_helper.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/resources/android/theme_resources.h"
#include "components/url_formatter/elide_url.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::AccessorySheetData;
using autofill::AccessorySheetField;
using autofill::FooterCommand;
using autofill::PasskeySection;
using autofill::PlusAddressInfo;
using autofill::UserInfo;
using autofill::mojom::FocusedFieldType;
using password_manager::CredentialCache;
using password_manager::UiCredential;
using plus_addresses::PlusProfile;
using webauthn::WebAuthnCredManDelegate;
using BlocklistedStatus =
    password_manager::OriginCredentialStore::BlocklistedStatus;
using FillingSource = ManualFillingController::FillingSource;
using IsExactMatch = autofill::UserInfo::IsExactMatch;
using ShouldShowAction = ManualFillingController::ShouldShowAction;

namespace {

autofill::UserInfo TranslateCredentials(const UiCredential& credential,
                                        const url::Origin& frame_origin,
                                        bool current_field_is_password,
                                        int username_icon_id) {
  DCHECK(!credential.origin().opaque());
  UserInfo user_info(
      credential.origin().Serialize(),
      IsExactMatch(credential.match_type() ==
                   password_manager_util::GetLoginMatchType::kExact));

  std::u16string username = GetDisplayUsername(credential);
  user_info.add_field(AccessorySheetField::Builder()
                          .SetDisplayText(username)
                          .SetSelectable(!credential.username().empty())
                          .SetIconId(username_icon_id)
                          .Build());

  user_info.add_field(
      AccessorySheetField::Builder()
          .SetDisplayText(credential.password())
          .SetA11yDescription(l10n_util::GetStringFUTF16(
              IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, username))
          .SetIsObfuscated(true)
          .SetSelectable(current_field_is_password)
          .Build());

  return user_info;
}

std::u16string GetPasswordTitle(bool has_credentials,
                                bool has_standalone_plus_addresses,
                                const url::Origin& origin) {
  const std::u16string elided_url =
      url_formatter::FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  if (!has_credentials) {
    return l10n_util::GetStringFUTF16(
        IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE, elided_url);
  }
  return has_standalone_plus_addresses
             ? l10n_util::GetStringFUTF16(
                   IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE,
                   elided_url)
             : std::u16string();
}

std::u16string GetPlusAddressTitle(bool has_standalone_plus_addresses,
                                   const url::Origin& origin) {
  const std::u16string elided_url =
      url_formatter::FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  return has_standalone_plus_addresses
             ? l10n_util::GetStringFUTF16(
                   IDS_PLUS_ADDRESS_FALLBACK_MANUAL_FILLING_SHEET_TITLE,
                   elided_url)
             : std::u16string();
}

password_manager::PasswordManagerDriver* GetPasswordManagerDriver(
    content::WebContents* web_contents) {
  if (content::RenderFrameHost* rfh = web_contents->GetFocusedFrame()) {
    return password_manager::ContentPasswordManagerDriver::
        GetForRenderFrameHost(rfh);
  }
  return nullptr;  // No driver without focused frame!
}

ShouldShowAction ShouldShowCredManReentryAction(
    FocusedFieldType focused_field_type,
    bool has_pending_credman_flow) {
  if (!has_pending_credman_flow) {
    return ShouldShowAction(false);
  }
  switch (focused_field_type) {
    case FocusedFieldType::kFillablePasswordField:
    case FocusedFieldType::kFillableUsernameField:
    case FocusedFieldType::kFillableWebauthnTaggedField:
      return ShouldShowAction(true);
    case FocusedFieldType::kFillableNonSearchField:
    case FocusedFieldType::kFillableSearchField:
    case FocusedFieldType::kFillableTextArea:
    case FocusedFieldType::kUnfillableElement:
    case FocusedFieldType::kUnknown:
      return ShouldShowAction(false);
  }
  NOTREACHED() << "Showing undefined for " << focused_field_type;
}

}  // namespace

PasswordAccessoryControllerImpl::~PasswordAccessoryControllerImpl() {
  if (authenticator_) {
    authenticator_->Cancel();
  }
  if (plus_profiles_provider_) {
    plus_profiles_provider_->RemoveObserver(this);
  }
}

void PasswordAccessoryControllerImpl::RegisterFillingSourceObserver(
    FillingSourceObserver observer) {
  source_observer_ = std::move(observer);
}

std::optional<AccessorySheetData>
PasswordAccessoryControllerImpl::GetSheetData() const {
  // Prevent crashing by returning a nullopt if no field was focused yet or if
  // the frame was (possibly temporarily) unfocused. This signals to the caller
  // that no sheet is available right now.
  if (GetWebContents().GetFocusedFrame() == nullptr) {
    return std::nullopt;
  }
  if (!last_focused_field_info_) {
    return std::nullopt;
  }
  url::Origin origin = GetFocusedFrameOrigin();
  // If the focused origin doesn't match the last known origin, it is not safe
  // to provide any suggestions (because e.g. information about field type isn't
  // reliable).
  if (!last_focused_field_info_->origin.IsSameOriginWith(origin)) {
    return std::nullopt;
  }

  std::vector<PasskeySection> passkeys_to_add;
  std::vector<UserInfo> info_to_add;
  std::vector<autofill::PlusAddressInfo> plus_address_info_to_add;
  base::span<const PlusProfile> plus_profiles =
      plus_profiles_provider_
          ? plus_profiles_provider_->GetAffiliatedPlusProfiles()
          : base::span<const PlusProfile, 0>();

  base::flat_map<std::string, bool>::container_type items(plus_profiles.size());
  for (const PlusProfile& profile : plus_profiles) {
    items.push_back({*profile.plus_address, false});
  }
  base::flat_map<std::string, bool> plus_addresses_used_as_usernames(
      std::move(items));

  const bool is_password_field = last_focused_field_info_->focused_field_type ==
                                 FocusedFieldType::kFillablePasswordField;
  if (autofill::IsFillable(last_focused_field_info_->focused_field_type)) {
    base::span<const UiCredential> suggestions =
        credential_cache_->GetCredentialStore(origin).GetCredentials();
    info_to_add.reserve(suggestions.size());
    for (const auto& credential : suggestions) {
      const std::string username_utf8 =
          base::UTF16ToUTF8(credential.username());
      int username_icon_id = 0;
      if (auto it = plus_addresses_used_as_usernames.find(username_utf8);
          it != plus_addresses_used_as_usernames.end()) {
        it->second = true;
        username_icon_id =
            ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_PLUS_ADDRESS);
      }
      info_to_add.emplace_back(TranslateCredentials(
          credential, origin, is_password_field, username_icon_id));
    }
  }

  for (const PlusProfile& profile : plus_profiles) {
    if (!plus_addresses_used_as_usernames[*profile.plus_address]) {
      plus_address_info_to_add.emplace_back(
          autofill::PlusAddressInfo(profile.facet.canonical_spec(),
                                    base::UTF8ToUTF16(*profile.plus_address)));
    }
  }

  if (password_manager::PasswordManagerDriver* driver =
          driver_supplier_.Run((&GetWebContents()))) {
    if (password_manager::WebAuthnCredentialsDelegate* credentials_delegate =
            password_client_->GetWebAuthnCredentialsDelegateForDriver(driver)) {
      if (auto passkeys = credentials_delegate->GetPasskeys()) {
        passkeys_to_add.reserve(passkeys->size());
        for (const password_manager::PasskeyCredential& passkey :
             passkeys.value()) {
          passkeys_to_add.emplace_back(passkey.display_name(),
                                       passkey.credential_id());
        }
      }
    }
  }

  bool has_suggestions = !info_to_add.empty() || !passkeys_to_add.empty();
  AccessorySheetData data = autofill::CreateAccessorySheetData(
      autofill::AccessoryTabType::PASSWORDS,
      GetPasswordTitle(has_suggestions, !plus_address_info_to_add.empty(),
                       origin),
      GetPlusAddressTitle(!plus_address_info_to_add.empty(), origin),
      std::move(info_to_add), CreateManagePasswordsFooter());
  base::ranges::for_each(std::move(passkeys_to_add),
                         [&data](PasskeySection section) {
                           data.add_passkey_section(std::move(section));
                         });
  base::ranges::for_each(
      std::move(plus_address_info_to_add),
      [&data](autofill::PlusAddressInfo plus_address_info) {
        data.add_plus_address_info(std::move(plus_address_info));
      });

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
  authenticator_ = password_client_->GetDeviceAuthenticator();
  if (!ShouldTriggerBiometricReauth(selection)) {
    authenticator_.reset();
    FillSelection(selection);
    return;
  }

  // |this| cancels the authentication when it is destroyed if one is ongoing,
  // which resets the callback, so it's safe to use base::Unretained(this) here.
  authenticator_->AuthenticateWithMessage(
      u"", base::BindOnce(&PasswordAccessoryControllerImpl::OnReauthCompleted,
                          base::Unretained(this), selection));
}

void PasswordAccessoryControllerImpl::OnPasskeySelected(
    const std::vector<uint8_t>& passkey_id) {
  if (password_manager::PasswordManagerDriver* driver =
          driver_supplier_.Run((&GetWebContents()))) {
    if (password_manager::WebAuthnCredentialsDelegate* credentials_delegate =
            password_client_->GetWebAuthnCredentialsDelegateForDriver(driver)) {
      credentials_delegate->SelectPasskey(base::Base64Encode(passkey_id),
                                          base::DoNothing());
    }
  }
}

// static
PasswordAccessoryController* PasswordAccessoryController::GetOrCreate(
    content::WebContents* web_contents,
    CredentialCache* credential_cache) {
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
    CredentialCache* credential_cache) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(credential_cache);

  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new PasswordAccessoryControllerImpl(
            web_contents, credential_cache, nullptr,
            ChromePasswordManagerClient::FromWebContents(web_contents),
            base::BindRepeating(GetPasswordManagerDriver),
            base::BindRepeating(&local_password_migration::ShowWarning),
            std::make_unique<PasswordAccessLossWarningBridgeImpl>())));
  }
}

// static
void PasswordAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    CredentialCache* credential_cache,
    base::WeakPtr<ManualFillingController> manual_filling_controller,
    password_manager::PasswordManagerClient* password_client,
    PasswordDriverSupplierForFocusedFrame driver_supplier,
    ShowMigrationWarningCallback show_migration_warning_callback,
    std::unique_ptr<PasswordAccessLossWarningBridge>
        access_loss_warning_bridge) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(manual_filling_controller);
  DCHECK(password_client);

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new PasswordAccessoryControllerImpl(
          web_contents, credential_cache, std::move(manual_filling_controller),
          password_client, std::move(driver_supplier),
          std::move(show_migration_warning_callback),
          std::move(access_loss_warning_bridge))));
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
        WebAuthnCredManDelegate* delegate =
            password_client_->GetWebAuthnCredManDelegateForDriver(driver);
        if (!delegate) {
          return;
        }
        switch (WebAuthnCredManDelegate::CredManMode()) {
          case WebAuthnCredManDelegate::CredManEnabledMode::kAllCredMan:
            delegate->TriggerCredManUi(
                WebAuthnCredManDelegate::RequestPasswords(true));
            return;
          case WebAuthnCredManDelegate::CredManEnabledMode::kNonGpmPasskeys:
            delegate->TriggerCredManUi(
                WebAuthnCredManDelegate::RequestPasswords(false));
            return;
          default:
            NOTREACHED_IN_MIGRATION()
                << "WebAuthnCredManDelegate should not be used if "
                   "CredManMode is kNotEnabled!";
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
          CHECK(credentials_delegate->IsSecurityKeyOrHybridFlowAvailable());
          credentials_delegate->LaunchSecurityKeyOrHybridFlow();
        }
      }
      return;
    case autofill::AccessoryAction::CREATE_PLUS_ADDRESS_FROM_PASSWORD_SHEET:
      if (auto* client = autofill::ContentAutofillClient::FromWebContents(
              &GetWebContents())) {
        client->OfferPlusAddressCreation(
            client->GetLastCommittedPrimaryMainFrameOrigin(),
            base::BindOnce(
                &PasswordAccessoryControllerImpl::OnPlusAddressCreated,
                weak_ptr_factory_.GetWeakPtr()));
        GetManualFillingController()->Hide();
      }
      return;
    case autofill::AccessoryAction::SELECT_PLUS_ADDRESS_FROM_PASSWORD_SHEET:
      all_plus_addresses_bottom_sheet_controller_ = std::make_unique<
          plus_addresses::AllPlusAddressesBottomSheetController>(
          &GetWebContents());
      all_plus_addresses_bottom_sheet_controller_->Show(base::BindOnce(
          &PasswordAccessoryControllerImpl::OnPlusAddressSelected,
          weak_ptr_factory_.GetWeakPtr()));
      GetManualFillingController()->Hide();
      return;
    case autofill::AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_PASSWORD_SHEET:
      plus_addresses::ShowManagePlusAddressesPage(GetWebContents());
      return;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unhandled selected action: " << static_cast<int>(selected_action);
  }
}

void PasswordAccessoryControllerImpl::OnToggleChanged(
    autofill::AccessoryAction toggled_action,
    bool enabled) {
  if (toggled_action == autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS) {
    ChangeCurrentOriginSavePasswordsStatus(enabled);
    return;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unhandled selected action: " << static_cast<int>(toggled_action);
}

void PasswordAccessoryControllerImpl::RegisterPlusProfilesProvider(
    base::WeakPtr<AffiliatedPlusProfilesProvider> provider) {
  plus_profiles_provider_ = provider;
  if (plus_profiles_provider_) {
    plus_profiles_provider_->AddObserver(this);
  }
}

void PasswordAccessoryControllerImpl::RefreshSuggestionsForField(
    FocusedFieldType focused_field_type) {
  // Discard all frame data. This ensures that the data is never used for an
  // incorrect frame.
  last_focused_field_info_ = std::nullopt;
  all_passwords_helper_.SetLastFocusedFieldType(focused_field_type);

  // Prevent crashing by not acting at all if frame became unfocused at any
  // point. The next time a focus event happens, this will be called again and
  // ensure we show correct data.
  if (GetWebContents().GetFocusedFrame() == nullptr) {
    return;
  }
  url::Origin origin = GetFocusedFrameOrigin();
  if (origin.opaque()) {
    return;  // Don't proceed for invalid origins.
  }
  password_manager::PasswordManagerDriver* driver =
      driver_supplier_.Run(&GetWebContents());
  if (!driver) {
    return;
  }
  TRACE_EVENT0("passwords",
               "PasswordAccessoryControllerImpl::RefreshSuggestionsForField");
  const bool is_manual_generation_available =
      password_manager_util::ManualPasswordGenerationEnabled(driver) &&
      password_client_->GetPasswordManager()->HaveFormManagersReceivedData(
          driver);

  last_focused_field_info_.emplace(origin, focused_field_type,
                                   is_manual_generation_available);

  RefreshSuggestions();
}

void PasswordAccessoryControllerImpl::OnGenerationRequested(
    autofill::password_generation::PasswordGenerationType type) {
  PasswordGenerationController* pwd_generation_controller =
      PasswordGenerationController::GetIfExisting(&GetWebContents());

  DCHECK(pwd_generation_controller);
  pwd_generation_controller->OnGenerationRequested(type);
}

void PasswordAccessoryControllerImpl::UpdateCredManReentryUi(
    FocusedFieldType focused_field_type) {
  if (WebAuthnCredManDelegate::CredManMode() ==
      WebAuthnCredManDelegate::CredManEnabledMode::kNotEnabled) {
    return;  // No updates required.
  }
  if (password_manager::PasswordManagerDriver* driver =
          driver_supplier_.Run(&GetWebContents())) {
    if (WebAuthnCredManDelegate* delegate =
            password_client_->GetWebAuthnCredManDelegateForDriver(driver)) {
      GetManualFillingController()->OnAccessoryActionAvailabilityChanged(
          ShouldShowCredManReentryAction(
              focused_field_type,
              delegate->HasPasskeys() == WebAuthnCredManDelegate::kHasPasskeys),
          autofill::AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY);
    }
  }
}

base::WeakPtr<PasswordAccessoryController>
PasswordAccessoryControllerImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

PasswordAccessoryControllerImpl::LastFocusedFieldInfo::LastFocusedFieldInfo(
    url::Origin focused_origin,
    FocusedFieldType focused_field,
    bool manual_generation_available)
    : origin(focused_origin),
      focused_field_type(focused_field),
      is_manual_generation_available(manual_generation_available) {}

PasswordAccessoryControllerImpl::PasswordAccessoryControllerImpl(
    content::WebContents* web_contents,
    CredentialCache* credential_cache,
    base::WeakPtr<ManualFillingController> manual_filling_controller,
    password_manager::PasswordManagerClient* password_client,
    PasswordDriverSupplierForFocusedFrame driver_supplier,
    ShowMigrationWarningCallback show_migration_warning_callback,
    std::unique_ptr<PasswordAccessLossWarningBridge> access_loss_warning_bridge)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PasswordAccessoryControllerImpl>(
          *web_contents),
      credential_cache_(credential_cache),
      manual_filling_controller_(std::move(manual_filling_controller)),
      password_client_(password_client),
      driver_supplier_(std::move(driver_supplier)),
      show_migration_warning_callback_(
          std::move(show_migration_warning_callback)),
      access_loss_warning_bridge_(std::move(access_loss_warning_bridge)),
      plus_address_service_(PlusAddressServiceFactory::GetForBrowserContext(
          GetWebContents().GetBrowserContext())) {}

std::vector<FooterCommand>
PasswordAccessoryControllerImpl::CreateManagePasswordsFooter() const {
  std::vector<FooterCommand> footer_commands_to_add;
  bool has_passkeys = false;
  if (password_manager::PasswordManagerDriver* driver =
          driver_supplier_.Run((&GetWebContents()))) {
    if (webauthn::WebAuthnCredManDelegate::CredManMode() !=
        webauthn::WebAuthnCredManDelegate::kNotEnabled) {
      if (auto* delegate =
              password_client_->GetWebAuthnCredManDelegateForDriver(driver)) {
        has_passkeys |= delegate->HasPasskeys();
        if (delegate->HasPasskeys()) {
          footer_commands_to_add.emplace_back(
              l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_ACCESSORY_SELECT_PASSKEY),
              autofill::AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY);
        }
      }
    }
  }

  if (all_passwords_helper_.available_credentials().has_value() &&
      IsSecureSite() &&
      GetFocusedFrameOrigin().GetURL().SchemeIsCryptographic() &&
      all_passwords_helper_.available_credentials().value() > 0) {
    footer_commands_to_add.emplace_back(
        l10n_util::GetStringUTF16(
            IDS_PASSWORD_MANAGER_ACCESSORY_SELECT_PASSWORD),
        autofill::AccessoryAction::USE_OTHER_PASSWORD);
  }

  const bool is_password_field = last_focused_field_info_->focused_field_type ==
                                 FocusedFieldType::kFillablePasswordField;
  if (is_password_field &&
      last_focused_field_info_->is_manual_generation_available) {
    std::u16string generate_password_title = l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_ACCESSORY_GENERATE_PASSWORD_BUTTON_TITLE);
    footer_commands_to_add.emplace_back(
        generate_password_title,
        autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL);
  }

  if (password_manager::PasswordManagerDriver* driver =
          driver_supplier_.Run((&GetWebContents()))) {
    if (password_manager::WebAuthnCredentialsDelegate* credentials_delegate =
            password_client_->GetWebAuthnCredentialsDelegateForDriver(driver)) {
      has_passkeys |= credentials_delegate->GetPasskeys() &&
                      !credentials_delegate->GetPasskeys()->empty();
      if (credentials_delegate->IsSecurityKeyOrHybridFlowAvailable()) {
        std::u16string passkey_other_device_title = l10n_util::GetStringUTF16(
            IDS_PASSWORD_MANAGER_ACCESSORY_USE_DEVICE_PASSKEY);
        footer_commands_to_add.emplace_back(
            passkey_other_device_title,
            autofill::AccessoryAction::CROSS_DEVICE_PASSKEY);
      }
    }
  }

  auto manage_passwords_message_id =
      has_passkeys
          ? IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_AND_PASSKEYS_LINK
          : IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK;
  std::u16string manage_passwords_title =
      l10n_util::GetStringUTF16(manage_passwords_message_id);
  footer_commands_to_add.emplace_back(
      manage_passwords_title, autofill::AccessoryAction::MANAGE_PASSWORDS);

  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled)) {
    // Both `ContentAutofillClient and this controller are instances of the
    // `WebContentsUserData`. There's no well-defined destruction order between
    // two different `WebContentsUserData` objects. That's why
    // `ContentAutofillClient` cannot be stored in a `raw_ptr` member variable
    // like `PlusAddressService`.
    auto* autofill_client =
        autofill::ContentAutofillClient::FromWebContents(&GetWebContents());
    if (autofill_client && plus_address_service_) {
      // Offer plus address creation if it's supported for the current user
      // session and if the user doesn't have any plus addresses created for the
      // current domain.
      if (plus_address_service_->IsPlusAddressCreationEnabled(
              autofill_client->GetLastCommittedPrimaryMainFrameOrigin(),
              autofill_client->IsOffTheRecord()) &&
          plus_profiles_provider_ &&
          plus_profiles_provider_->GetAffiliatedPlusProfiles().empty()) {
        footer_commands_to_add.emplace_back(
            l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_CREATE_NEW_PLUS_ADDRESSES_LINK_ANDROID),
            autofill::AccessoryAction::CREATE_PLUS_ADDRESS_FROM_PASSWORD_SHEET);
      }
      // Offer the user to select the plus address manually if plus address
      // filling is supported for the last committed origin and the user has at
      // least 1 plus address.
      if (plus_address_service_->IsPlusAddressFillingEnabled(
              autofill_client->GetLastCommittedPrimaryMainFrameOrigin()) &&
          !plus_address_service_->GetPlusProfiles().empty()) {
        footer_commands_to_add.emplace_back(
            l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_SELECT_PLUS_ADDRESS_LINK_ANDROID),
            autofill::AccessoryAction::SELECT_PLUS_ADDRESS_FROM_PASSWORD_SHEET);
      }
      // Show "Manage plus addresses" action only if the user has at least 1
      // affiliated plus addresses already saved for the current domain.
      if (plus_profiles_provider_ &&
          !plus_profiles_provider_->GetAffiliatedPlusProfiles().empty()) {
        footer_commands_to_add.emplace_back(FooterCommand(
            l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_LINK_ANDROID),
            autofill::AccessoryAction::
                MANAGE_PLUS_ADDRESS_FROM_PASSWORD_SHEET));
      }
    }
  }

  return footer_commands_to_add;
}

void PasswordAccessoryControllerImpl::WebContentsDestroyed() {
  // Remove itself to avoid that pointers to other `WebContentsUserData` objects
  // become invalid.
  GetWebContents().RemoveUserData(UserDataKey());
  // Do not add code - `this` is now destroyed.
}

void PasswordAccessoryControllerImpl::ChangeCurrentOriginSavePasswordsStatus(
    bool saving_enabled) {
  const url::Origin origin = GetFocusedFrameOrigin();
  if (origin.opaque()) {
    return;
  }

  const GURL origin_as_gurl = origin.GetURL();
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml,
      password_manager::GetSignonRealm(origin_as_gurl), origin_as_gurl);

  password_manager::PasswordStoreInterface* store;
  if (password_client_->GetPasswordFeatureManager()
          ->IsOptedInForAccountStorage() &&
      password_client_->GetPasswordFeatureManager()
              ->GetDefaultPasswordStore() ==
          password_manager::PasswordForm::Store::kAccountStore) {
    store = password_client_->GetAccountPasswordStore();
  } else {
    store = password_client_->GetProfilePasswordStore();
  }

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
  if (origin.opaque()) {
    return false;  // Don't proceed for invalid origins.
  }

  // If the `suggestion` to fill is a valid plus address, it can be filled.
  if (plus_address_service_ &&
      plus_address_service_->IsPlusAddress(base::UTF16ToUTF8(suggestion))) {
    return true;
  }

  return base::ranges::any_of(
      credential_cache_->GetCredentialStore(origin).GetCredentials(),
      [&](const auto& cred) {
        return suggestion == (is_password ? cred.password() : cred.username());
      });
}

bool PasswordAccessoryControllerImpl::ShouldShowRecoveryToggle(
    const url::Origin& origin) const {
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
  // TODO(crbug.com/40815830): Investigate if focused frame really needs
  // to return RenderFrameHosts with non-live RenderFrames.
  if (!GetWebContents().GetFocusedFrame()->IsRenderFrameLive()) {
    return;
  }

  // We can use |base::Unretained| safely because at the time of calling
  // |AllPasswordsSheetDismissed| we are sure that this controller is alive as
  // it owns |AllPasswordsBottomSheetController| from which the method is
  // called.
  // TODO(crbug.com/40139552): Update the controller with the last focused
  // field.
  all_passords_bottom_sheet_controller_ =
      std::make_unique<AllPasswordsBottomSheetController>(
          &GetWebContents(), password_client_->GetProfilePasswordStore(),
          password_client_->GetAccountPasswordStore(),
          base::BindOnce(
              &PasswordAccessoryControllerImpl::AllPasswordsSheetDismissed,
              base::Unretained(this)),
          last_focused_field_info_->focused_field_type);

  all_passords_bottom_sheet_controller_->Show();
}

bool PasswordAccessoryControllerImpl::ShouldTriggerBiometricReauth(
    const AccessorySheetField& selection) const {
  if (!selection.is_obfuscated()) {
    return false;
  }

  return password_client_->IsReauthBeforeFillingRequired(authenticator_.get());
}

void PasswordAccessoryControllerImpl::OnReauthCompleted(
    AccessorySheetField selection,
    bool auth_succeeded) {
  authenticator_.reset();
  if (!auth_succeeded) {
    return;
  }
  FillSelection(selection);
}

void PasswordAccessoryControllerImpl::FillSelection(
    const AccessorySheetField& selection) {
  if (!AppearsInSuggestions(selection.display_text(), selection.is_obfuscated(),
                            GetFocusedFrameOrigin())) {
    NOTREACHED_IN_MIGRATION() << "Tried to fill '" << selection.display_text()
                              << "' into " << GetFocusedFrameOrigin();
    return;  // Never fill across different origins!
  }
  password_manager::PasswordManagerDriver* driver =
      driver_supplier_.Run(&GetWebContents());
  if (!driver) {
    return;
  }
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
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning)) {
    Profile* profile =
        Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
    if (profile && access_loss_warning_bridge_->ShouldShowAccessLossNoticeSheet(
                       profile->GetPrefs(), /*called_at_startup=*/false)) {
      access_loss_warning_bridge_->MaybeShowAccessLossNoticeSheet(
          profile->GetPrefs(), GetWebContents().GetTopLevelNativeWindow(),
          profile, /*called_at_startup=*/false,
          password_manager_android_util::PasswordAccessLossWarningTriggers::
              kKeyboardAcessorySheet);
    }
  }
}

void PasswordAccessoryControllerImpl::AllPasswordsSheetDismissed() {
  all_passords_bottom_sheet_controller_.reset();
}

void PasswordAccessoryControllerImpl::OnPlusAddressCreated(
    const std::string& plus_address) {
  password_manager::PasswordManagerDriver* driver =
      driver_supplier_.Run(&GetWebContents());
  if (!driver) {
    return;
  }
  driver->FillIntoFocusedField(/*is_password=*/false,
                               base::UTF8ToUTF16(plus_address));
}

void PasswordAccessoryControllerImpl::OnPlusAddressSelected(
    base::optional_ref<const std::string> plus_address) {
  all_plus_addresses_bottom_sheet_controller_.reset();
  if (!plus_address) {
    return;
  }
  if (password_manager::PasswordManagerDriver* driver =
          driver_supplier_.Run(&GetWebContents())) {
    driver->FillIntoFocusedField(/*is_password=*/false,
                                 base::UTF8ToUTF16(plus_address.value()));
  }
}

void PasswordAccessoryControllerImpl::RefreshSuggestions() {
  if (!last_focused_field_info_) {
    return;
  }

  bool sheet_provides_value =
      last_focused_field_info_->is_manual_generation_available;

  all_passwords_helper_.ClearUpdateCallback();
  if (!all_passwords_helper_.available_credentials().has_value()) {
    all_passwords_helper_.SetUpdateCallback(base::BindOnce(
        &PasswordAccessoryControllerImpl::RefreshSuggestionsForField,
        base::Unretained(this), last_focused_field_info_->focused_field_type));
  } else {
    sheet_provides_value |=
        all_passwords_helper_.available_credentials().value() > 0;
  }

  if (ShouldShowRecoveryToggle(last_focused_field_info_->origin)) {
    if (credential_cache_->GetCredentialStore(last_focused_field_info_->origin)
            .GetBlocklistedStatus() == BlocklistedStatus::kIsBlocklisted) {
      UMA_HISTOGRAM_BOOLEAN(
          "KeyboardAccessory.DisabledSavingAccessoryImpressions", true);
    }
    sheet_provides_value = true;
  }

  // The all passwords sheet could cover this but if it's still loading, use
  // this data as the next closest proxy to minimize delayed updates UI.
  sheet_provides_value |=
      !credential_cache_->GetCredentialStore(last_focused_field_info_->origin)
           .GetCredentials()
           .empty();

  if (plus_profiles_provider_) {
    sheet_provides_value |=
        !plus_profiles_provider_->GetAffiliatedPlusProfiles().empty();
  }

  CHECK(source_observer_);
  // The "Manage Passwords" entry point doesn't justify showing this fallback
  // sheet for non-password fields.
  source_observer_.Run(
      this,
      IsFillingSourceAvailable(
          autofill::IsFillable(last_focused_field_info_->focused_field_type) &&
          sheet_provides_value));
}

void PasswordAccessoryControllerImpl::OnAffiliatedPlusProfilesFetched() {
  RefreshSuggestions();
}

bool PasswordAccessoryControllerImpl::IsSecureSite() const {
  if (security_level_for_testing_) {
    return security_level_for_testing_ == security_state::SECURE;
  }

  ChromeSecurityStateTabHelper::CreateForWebContents(&GetWebContents());
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
