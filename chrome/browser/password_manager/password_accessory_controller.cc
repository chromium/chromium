// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_accessory_controller.h"

#include <vector>

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/preferences/preferences_launcher.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/password_manager/password_accessory_metrics_util.h"
#include "chrome/browser/password_manager/password_generation_dialog_view_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

using autofill::PasswordForm;
using Item = PasswordAccessoryViewInterface::AccessoryItem;

namespace {

void RecordGenerationDialogDismissal(bool accepted) {
  UMA_HISTOGRAM_BOOLEAN("KeyboardAccessory.GeneratedPasswordDialog", accepted);
}

}  // namespace

struct PasswordAccessoryController::GenerationElementData {
  GenerationElementData(autofill::PasswordForm form,
                        autofill::FormSignature form_signature,
                        autofill::FieldSignature field_signature,
                        uint32_t max_password_length)
      : form(std::move(form)),
        form_signature(form_signature),
        field_signature(field_signature),
        max_password_length(max_password_length) {}

  // Form for which password generation is triggered.
  autofill::PasswordForm form;

  // Signature of the form for which password generation is triggered.
  autofill::FormSignature form_signature;

  // Signature of the field for which password generation is triggered.
  autofill::FieldSignature field_signature;

  // Maximum length of the generated password.
  uint32_t max_password_length;
};

struct PasswordAccessoryController::SuggestionElementData {
  SuggestionElementData(base::string16 password,
                        base::string16 username,
                        Item::Type username_type)
      : password(password), username(username), username_type(username_type) {}

  // Password string to be used for this credential.
  base::string16 password;

  // Username string to be used for this credential.
  base::string16 username;

  // Decides whether the username is interactive (i.e. empty ones are not).
  Item::Type username_type;
};

struct PasswordAccessoryController::FaviconRequestData {
  // List of requests waiting for favicons to be available.
  std::vector<base::OnceCallback<void(const gfx::Image&)>> pending_requests;

  // Cached image for this origin. |IsEmpty()| unless a favicon was found.
  gfx::Image cached_icon;
};

PasswordAccessoryController::PasswordAccessoryController(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      view_(PasswordAccessoryViewInterface::Create(this)),
      create_dialog_factory_(
          base::BindRepeating(&PasswordGenerationDialogViewInterface::Create)),
      favicon_service_(FaviconServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          ServiceAccessType::EXPLICIT_ACCESS)),
      weak_factory_(this) {}

// Additional creation functions in unit tests only:
PasswordAccessoryController::PasswordAccessoryController(
    content::WebContents* web_contents,
    std::unique_ptr<PasswordAccessoryViewInterface> view,
    CreateDialogFactory create_dialog_factory,
    favicon::FaviconService* favicon_service)
    : web_contents_(web_contents),
      view_(std::move(view)),
      create_dialog_factory_(create_dialog_factory),
      favicon_service_(favicon_service),
      weak_factory_(this) {}

PasswordAccessoryController::~PasswordAccessoryController() = default;

// static
bool PasswordAccessoryController::AllowedForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  if (vr::VrTabHelper::IsInVr(web_contents)) {
    return false;  // TODO(crbug.com/865749): Reenable if works for VR keyboard.
  }
  // Either #passwords-keyboards-accessory or #experimental-ui must be enabled.
  return base::FeatureList::IsEnabled(
             password_manager::features::kPasswordsKeyboardAccessory) ||
         base::FeatureList::IsEnabled(features::kExperimentalUi);
}

// static
void PasswordAccessoryController::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    std::unique_ptr<PasswordAccessoryViewInterface> view,
    CreateDialogFactory create_dialog_factory,
    favicon::FaviconService* favicon_service) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  web_contents->SetUserData(
      UserDataKey(), base::WrapUnique(new PasswordAccessoryController(
                         web_contents, std::move(view), create_dialog_factory,
                         favicon_service)));
}

void PasswordAccessoryController::SavePasswordsForOrigin(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const url::Origin& origin) {
  std::vector<SuggestionElementData>* suggestions =
      &origin_suggestions_[origin];
  suggestions->clear();
  for (const auto& pair : best_matches) {
    const PasswordForm* form = pair.second;
    suggestions->emplace_back(form->password_value, GetDisplayUsername(*form),
                              Item::Type::NON_INTERACTIVE_SUGGESTION);
  }
}

void PasswordAccessoryController::OnAutomaticGenerationStatusChanged(
    bool available,
    const base::Optional<
        autofill::password_generation::PasswordGenerationUIData>& ui_data,
    const base::WeakPtr<password_manager::PasswordManagerDriver>& driver) {
  target_frame_driver_ = driver;
  if (available) {
    DCHECK(ui_data.has_value());
    generation_element_data_ = std::make_unique<GenerationElementData>(
        ui_data.value().password_form,
        autofill::CalculateFormSignature(
            ui_data.value().password_form.form_data),
        autofill::CalculateFieldSignatureByNameAndType(
            ui_data.value().generation_element, "password"),
        ui_data.value().max_length);
  } else {
    generation_element_data_.reset();
  }
  DCHECK(view_);
  view_->OnAutomaticGenerationStatusChanged(available);
}

void PasswordAccessoryController::OnFilledIntoFocusedField(
    autofill::FillingStatus status) {
  if (status != autofill::FillingStatus::SUCCESS)
    return;                      // TODO(crbug/853766): Record success rate.
  view_->SwapSheetWithKeyboard();
}

void PasswordAccessoryController::RefreshSuggestionsForField(
    const url::Origin& origin,
    bool is_fillable,
    bool is_password_field) {
  if (is_fillable) {
    current_origin_ = origin;
    view_->OnItemsAvailable(CreateViewItems(origin, origin_suggestions_[origin],
                                            is_password_field));
    view_->SwapSheetWithKeyboard();
  } else {
    // For unfillable fields, reset the origin and send the empty state message.
    current_origin_ = url::Origin();
    view_->OnItemsAvailable(CreateViewItems(
        origin, std::vector<SuggestionElementData>(), is_password_field));
    view_->CloseAccessorySheet();
  }
}

void PasswordAccessoryController::DidNavigateMainFrame() {
  if (current_origin_.IsSameOriginWith(
          web_contents_->GetMainFrame()->GetLastCommittedOrigin()))
    return;  // Clean requests only if the navigation was across origins.
  favicon_tracker_.TryCancelAll();  // If there is a request pending, cancel it.
  current_origin_ = url::Origin();
  icons_request_data_.clear();
  origin_suggestions_.clear();
}

void PasswordAccessoryController::ShowWhenKeyboardIsVisible() {
  view_->ShowWhenKeyboardIsVisible();
}

void PasswordAccessoryController::Hide() {
  view_->Hide();
}

void PasswordAccessoryController::GetFavicon(
    int desired_size_in_pixel,
    base::OnceCallback<void(const gfx::Image&)> icon_callback) {
  url::Origin origin = current_origin_;  // Copy origin in case it changes.
  // Check whether this request can be immediately answered with a cached icon.
  // It is empty if there wasn't at least one request that found an icon yet.
  FaviconRequestData* icon_request = &icons_request_data_[origin];
  if (!icon_request->cached_icon.IsEmpty()) {
    std::move(icon_callback).Run(icon_request->cached_icon);
    return;
  }
  if (!favicon_service_) {  // This might happen in tests.
    std::move(icon_callback).Run(gfx::Image());
    return;
  }

  // The cache is empty. Queue the callback.
  icon_request->pending_requests.emplace_back(std::move(icon_callback));
  if (icon_request->pending_requests.size() > 1)
    return;  // The favicon for this origin was already requested.

  favicon_service_->GetRawFaviconForPageURL(
      origin.GetURL(), {favicon_base::IconType::kFavicon},
      desired_size_in_pixel,
      /* fallback_to_host = */ false,
      base::BindRepeating(  // FaviconService doesn't support BindOnce yet.
          &PasswordAccessoryController::OnImageFetched,
          weak_factory_.GetWeakPtr(), origin),
      &favicon_tracker_);
}

void PasswordAccessoryController::OnFillingTriggered(
    bool is_password,
    const base::string16& textToFill) {
  password_manager::ContentPasswordManagerDriverFactory* factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents_);
  DCHECK(factory);
  // TODO(fhorschig): Consider allowing filling on non-main frames.
  password_manager::ContentPasswordManagerDriver* driver =
      factory->GetDriverForFrame(web_contents_->GetMainFrame());
  if (!driver) {
    return;
  }  // |driver| can be NULL if the tab is being closed.
  driver->FillIntoFocusedField(
      is_password, textToFill,
      base::BindOnce(&PasswordAccessoryController::OnFilledIntoFocusedField,
                     weak_factory_.GetWeakPtr()));
}

void PasswordAccessoryController::OnOptionSelected(
    const base::string16& selectedOption) const {
  if (selectedOption ==
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK)) {
    UMA_HISTOGRAM_ENUMERATION("KeyboardAccessory.AccessoryActionSelected",
                              metrics::AccessoryAction::MANAGE_PASSWORDS,
                              metrics::AccessoryAction::COUNT);
    chrome::android::PreferencesLauncher::ShowPasswordSettings();
  }
}

void PasswordAccessoryController::OnGenerationRequested() {
  if (!target_frame_driver_)
    return;
  // TODO(crbug.com/835234): Take the modal dialog logic out of the accessory
  // controller.
  dialog_view_ = create_dialog_factory_.Run(this);
  uint32_t spec_priority = 0;
  base::string16 password =
      target_frame_driver_->GetPasswordGenerationManager()->GeneratePassword(
          web_contents_->GetLastCommittedURL().GetOrigin(),
          generation_element_data_->form_signature,
          generation_element_data_->field_signature,
          generation_element_data_->max_password_length, &spec_priority);
  if (target_frame_driver_ && target_frame_driver_->GetPasswordManager()) {
    target_frame_driver_->GetPasswordManager()
        ->ReportSpecPriorityForGeneratedPassword(generation_element_data_->form,
                                                 spec_priority);
  }
  dialog_view_->Show(password);
}

void PasswordAccessoryController::GeneratedPasswordAccepted(
    const base::string16& password) {
  if (!target_frame_driver_)
    return;
  RecordGenerationDialogDismissal(true);
  target_frame_driver_->GeneratedPasswordAccepted(password);
  dialog_view_.reset();
}

void PasswordAccessoryController::GeneratedPasswordRejected() {
  RecordGenerationDialogDismissal(false);
  dialog_view_.reset();
}

gfx::NativeView PasswordAccessoryController::container_view() const {
  return web_contents_->GetNativeView();
}

gfx::NativeWindow PasswordAccessoryController::native_window() const {
  return web_contents_->GetTopLevelNativeWindow();
}

// static
std::vector<Item> PasswordAccessoryController::CreateViewItems(
    const url::Origin& origin,
    const std::vector<SuggestionElementData>& suggestions,
    bool is_password_field) {
  std::vector<Item> items;
  base::string16 passwords_title_str;

  // Create a horizontal divider line before the title.
  items.emplace_back(base::string16(), base::string16(), false,
                     Item::Type::TOP_DIVIDER);

  // Create the title element
  passwords_title_str = l10n_util::GetStringFUTF16(
      suggestions.empty()
          ? IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE
          : IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE,
      base::ASCIIToUTF16(origin.host()));
  items.emplace_back(passwords_title_str, passwords_title_str,
                     /*is_password=*/false, Item::Type::LABEL);

  // Create a username and a password element for every suggestions
  for (const SuggestionElementData& suggestion : suggestions) {
    items.emplace_back(suggestion.username, suggestion.username,
                       /*is_password=*/false, suggestion.username_type);
    items.emplace_back(suggestion.password,
                       l10n_util::GetStringFUTF16(
                           IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION,
                           suggestion.username),
                       /*is_password=*/true,
                       is_password_field
                           ? Item::Type::SUGGESTION
                           : Item::Type::NON_INTERACTIVE_SUGGESTION);
  }

  // Create a horizontal divider line before the options.
  items.emplace_back(base::string16(), base::string16(), false,
                     Item::Type::DIVIDER);

  // Create the link to all passwords.
  base::string16 manage_passwords_title = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
  items.emplace_back(manage_passwords_title, manage_passwords_title, false,
                     Item::Type::OPTION);
  return items;
}

void PasswordAccessoryController::OnImageFetched(
    url::Origin origin,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  FaviconRequestData* icon_request = &icons_request_data_[origin];

  favicon_base::FaviconImageResult image_result;
  if (bitmap_result.is_valid()) {
    image_result.image = gfx::Image::CreateFrom1xPNGBytes(
        bitmap_result.bitmap_data->front(), bitmap_result.bitmap_data->size());
  }
  icon_request->cached_icon = image_result.image;
  // Only trigger all the callbacks if they still affect a displayed origin.
  if (origin == current_origin_) {
    for (auto& callback : icon_request->pending_requests) {
      std::move(callback).Run(icon_request->cached_icon);
    }
  }
  icon_request->pending_requests.clear();
}
