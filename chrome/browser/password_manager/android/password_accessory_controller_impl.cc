// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_accessory_controller_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::AccessorySheetData;
using autofill::FooterCommand;
using autofill::UserInfo;
using autofill::mojom::FocusedFieldType;
using password_manager::CredentialCache;
using password_manager::PasswordStore;
using password_manager::UiCredential;
using BlocklistedStatus =
    password_manager::OriginCredentialStore::BlocklistedStatus;
using FillingSource = ManualFillingController::FillingSource;
using IsPslMatch = autofill::UserInfo::IsPslMatch;

namespace {

autofill::UserInfo TranslateCredentials(bool current_field_is_password,
                                        const url::Origin& frame_origin,
                                        const UiCredential& credential) {
  DCHECK(!credential.origin().opaque());
  UserInfo user_info(credential.origin().Serialize(),
                     credential.is_public_suffix_match());

  std::u16string username = GetDisplayUsername(credential);
  user_info.add_field(
      UserInfo::Field(username, username, /*is_password=*/false,
                      /*selectable=*/!credential.username().empty() &&
                          !current_field_is_password));

  user_info.add_field(UserInfo::Field(
      credential.password(),
      l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, username),
      /*is_password=*/true, /*selectable=*/current_field_is_password));

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

}  // namespace

PasswordAccessoryControllerImpl::~PasswordAccessoryControllerImpl() = default;

void PasswordAccessoryControllerImpl::RegisterFillingSourceObserver(
    FillingSourceObserver observer) {
  source_observer_ = std::move(observer);
}

base::Optional<autofill::AccessorySheetData>
PasswordAccessoryControllerImpl::GetSheetData() const {
  // Prevent crashing by returning a nullopt if no field was focused yet or if
  // the frame was (possibly temporarily) unfocused. This signals to the caller
  // that no sheet is available right now.
  if (web_contents_->GetFocusedFrame() == nullptr)
    return base::nullopt;
  if (!last_focused_field_info_)
    return base::nullopt;
  url::Origin origin = GetFocusedFrameOrigin();
  // If the focused origin doesn't match the last known origin, it is not safe
  // to provide any suggestions (because e.g. information about field type isn't
  // reliable).
  if (!last_focused_field_info_->origin.IsSameOriginWith(origin))
    return base::nullopt;

  std::vector<UserInfo> info_to_add;
  std::vector<FooterCommand> footer_commands_to_add;
  const bool is_password_field = last_focused_field_info_->focused_field_type ==
                                 FocusedFieldType::kFillablePasswordField;

  if (autofill::IsFillable(last_focused_field_info_->focused_field_type)) {
    base::span<const UiCredential> suggestions =
        credential_cache_->GetCredentialStore(origin).GetCredentials();
    info_to_add.reserve(suggestions.size());
    for (const auto& credential : suggestions) {
      if (credential.is_public_suffix_match() &&
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
      all_passwords_helper_.available_credentials().value() > 0 &&
      base::FeatureList::IsEnabled(
          password_manager::features::kFillingPasswordsFromAnyOrigin)) {
    std::u16string button_title =
        is_password_field
            ? l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_ACCESSORY_USE_OTHER_PASSWORD)
            : l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_ACCESSORY_USE_OTHER_USERNAME);

    footer_commands_to_add.push_back(FooterCommand(
        button_title, autofill::AccessoryAction::USE_OTHER_PASSWORD));
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
    const autofill::UserInfo::Field& selection) {
  if (!AppearsInSuggestions(selection.display_text(), selection.is_obfuscated(),
                            GetFocusedFrameOrigin())) {
    NOTREACHED() << "Tried to fill '" << selection.display_text() << "' into "
                 << GetFocusedFrameOrigin();
    return;  // Never fill across different origins!
  }

  password_manager::ContentPasswordManagerDriverFactory* factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents_);
  password_manager::ContentPasswordManagerDriver* driver =
      factory->GetDriverForFrame(web_contents_->GetFocusedFrame());
  driver->FillIntoFocusedField(selection.is_obfuscated(),
                               selection.display_text());
}

// static
bool PasswordAccessoryController::AllowedForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  // TODO(crbug.com/902305): Re-enable if possible.
  return !vr::VrTabHelper::IsInVr(web_contents);
}

// static
PasswordAccessoryController* PasswordAccessoryController::GetOrCreate(
    content::WebContents* web_contents,
    password_manager::CredentialCache* credential_cache) {
  DCHECK(PasswordAccessoryController::AllowedForWebContents(web_contents));

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
            ChromePasswordManagerClient::FromWebContents(web_contents))));
  }
}

// static
void PasswordAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    password_manager::CredentialCache* credential_cache,
    base::WeakPtr<ManualFillingController> mf_controller,
    password_manager::PasswordManagerClient* password_client) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(mf_controller);
  DCHECK(password_client);

  web_contents->SetUserData(
      UserDataKey(), base::WrapUnique(new PasswordAccessoryControllerImpl(
                         web_contents, credential_cache,
                         std::move(mf_controller), password_client)));
}

// static
bool PasswordAccessoryControllerImpl::ShouldAcceptFocusEvent(
    content::WebContents* web_contents,
    password_manager::ContentPasswordManagerDriver* driver,
    FocusedFieldType focused_field_type) {
  // Only react to focus events that are sent for the current focused frame.
  // This is used to make sure that obsolette events that come in an unexpected
  // order are not processed. Example: (Frame1, focus) -> (Frame2, focus) ->
  // (Frame1, unfocus) would otherwise unset all the data set for Frame2, which
  // would be wrong.
  if (web_contents->GetFocusedFrame() &&
      driver->render_frame_host() == web_contents->GetFocusedFrame())
    return true;

  // The one event that is accepted even if there is no focused frame is an
  // "unfocus" event that resulted in all frames being unfocused. This can be
  // used to reset the state of the accessory.
  if (!web_contents->GetFocusedFrame() &&
      focused_field_type == FocusedFieldType::kUnknown)
    return true;
  return false;
}

void PasswordAccessoryControllerImpl::OnOptionSelected(
    autofill::AccessoryAction selected_action) {
  if (selected_action == autofill::AccessoryAction::USE_OTHER_PASSWORD) {
    ShowAllPasswords();
    return;
  }
  if (selected_action == autofill::AccessoryAction::MANAGE_PASSWORDS) {
    password_manager_launcher::ShowPasswordSettings(
        web_contents_,
        password_manager::ManagePasswordsReferrer::kPasswordsAccessorySheet);
    return;
  }
  if (selected_action == autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL) {
    OnGenerationRequested(
        autofill::password_generation::PasswordGenerationType::kManual);
    GetManualFillingController()->Hide();
    return;
  }
  if (selected_action ==
      autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC) {
    OnGenerationRequested(
        autofill::password_generation::PasswordGenerationType::kAutomatic);
    GetManualFillingController()->Hide();
    return;
  }
  NOTREACHED() << "Unhandled selected action: "
               << static_cast<int>(selected_action);
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
  last_focused_field_info_ = base::nullopt;
  all_passwords_helper_.SetLastFocusedFieldType(focused_field_type);

  // Prevent crashing by not acting at all if frame became unfocused at any
  // point. The next time a focus event happens, this will be called again and
  // ensure we show correct data.
  if (web_contents_->GetFocusedFrame() == nullptr)
    return;
  url::Origin origin = GetFocusedFrameOrigin();
  if (origin.opaque())
    return;  // Don't proceed for invalid origins.
  last_focused_field_info_.emplace(origin, focused_field_type,
                                   is_manual_generation_available);

  all_passwords_helper_.ClearUpdateCallback();
  if (!all_passwords_helper_.available_credentials().has_value()) {
    all_passwords_helper_.SetUpdateCallback(base::BindOnce(
        &PasswordAccessoryControllerImpl::RefreshSuggestionsForField,
        base::Unretained(this), focused_field_type,
        is_manual_generation_available));
  }

  if (ShouldShowRecoveryToggle(origin)) {
    if (credential_cache_->GetCredentialStore(origin).GetBlocklistedStatus() ==
        BlocklistedStatus::kIsBlocklisted) {
      UMA_HISTOGRAM_BOOLEAN(
          "KeyboardAccessory.DisabledSavingAccessoryImpressions", true);
    }
  }

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory)) {
    DCHECK(source_observer_);
    // The "Manage Passwords" entry point always justifies showing this fallback
    // sheet — given that the field is fillable at all.
    source_observer_.Run(this, IsFillingSourceAvailable(
                                   autofill::IsFillable(focused_field_type)));
  } else {
    base::Optional<AccessorySheetData> data = GetSheetData();
    DCHECK(data.has_value());
    GetManualFillingController()->RefreshSuggestions(std::move(data.value()));
  }
}

void PasswordAccessoryControllerImpl::OnGenerationRequested(
    autofill::password_generation::PasswordGenerationType type) {
  PasswordGenerationController* pwd_generation_controller =
      PasswordGenerationController::GetIfExisting(web_contents_);

  DCHECK(pwd_generation_controller);
  pwd_generation_controller->OnGenerationRequested(type);
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
    base::WeakPtr<ManualFillingController> mf_controller,
    password_manager::PasswordManagerClient* password_client)
    : web_contents_(web_contents),
      credential_cache_(credential_cache),
      mf_controller_(std::move(mf_controller)),
      password_client_(password_client) {}

void PasswordAccessoryControllerImpl::ChangeCurrentOriginSavePasswordsStatus(
    bool saving_enabled) {
  const url::Origin origin = GetFocusedFrameOrigin();
  if (origin.opaque())
    return;

  const GURL origin_as_gurl = origin.GetURL();
  password_manager::PasswordStore::FormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml,
      password_manager::GetSignonRealm(origin_as_gurl), origin_as_gurl);
  password_manager::PasswordStore* store =
      password_client_->GetProfilePasswordStore();
  if (saving_enabled) {
    store->Unblocklist(form_digest, base::NullCallback());
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
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(web_contents_);
  DCHECK(mf_controller_);
  return mf_controller_;
}

url::Origin PasswordAccessoryControllerImpl::GetFocusedFrameOrigin() const {
  if (web_contents_->GetFocusedFrame() == nullptr) {
    LOG(DFATAL) << "Tried to get retrieve origin without focused "
                   "frame.";
    return url::Origin();  // Nonce!
  }
  return web_contents_->GetFocusedFrame()->GetLastCommittedOrigin();
}

void PasswordAccessoryControllerImpl::ShowAllPasswords() {
  // If the controller is initialized that means that the UI is showing.
  if (all_passords_bottom_sheet_controller_ || !last_focused_field_info_) {
    return;
  }

  // We can use |base::Unretained| safely because at the time of calling
  // |AllPasswordsSheetDismissed| we are sure that this controller is alive as
  // it owns |AllPasswordsBottomSheetController| from which the method is
  // called.
  // TODO(crbug.com/1104132): Update the controller with the last focused field.
  all_passords_bottom_sheet_controller_ =
      std::make_unique<AllPasswordsBottomSheetController>(
          web_contents_, password_client_->GetProfilePasswordStore(),
          base::BindOnce(
              &PasswordAccessoryControllerImpl::AllPasswordsSheetDismissed,
              base::Unretained(this)),
          last_focused_field_info_->focused_field_type);

  all_passords_bottom_sheet_controller_->Show();
}

void PasswordAccessoryControllerImpl::AllPasswordsSheetDismissed() {
  all_passords_bottom_sheet_controller_.reset();
}

bool PasswordAccessoryControllerImpl::IsSecureSite() const {
  if (security_level_for_testing_) {
    return security_level_for_testing_ == security_state::SECURE;
  }

  SecurityStateTabHelper::CreateForWebContents(web_contents_);
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents_);

  return helper && helper->GetSecurityLevel() == security_state::SECURE;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PasswordAccessoryControllerImpl)
