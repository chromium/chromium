// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_controller_impl.h"

#include <utility>

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/autofill/address_accessory_controller.h"
#include "chrome/browser/autofill/credit_card_accessory_controller.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/browser/password_manager/password_accessory_metrics_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "content/public/browser/web_contents.h"

using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessoryTabType;
using autofill::AddressAccessoryController;
using autofill::CreditCardAccessoryController;
using autofill::mojom::FocusedFieldType;

using FillingSource = ManualFillingController::FillingSource;

namespace {

FillingSource GetSourceForTab(const AccessorySheetData& accessory_sheet) {
  switch (accessory_sheet.get_sheet_type()) {
    case AccessoryTabType::PASSWORDS:
      return FillingSource::PASSWORD_FALLBACKS;
    case AccessoryTabType::CREDIT_CARDS:
      return FillingSource::CREDIT_CARD_FALLBACKS;
    case AccessoryTabType::ADDRESSES:
      return FillingSource::ADDRESS_FALLBACKS;
    case AccessoryTabType::TOUCH_TO_FILL:
      return FillingSource::TOUCH_TO_FILL;
    case AccessoryTabType::ALL:
    case AccessoryTabType::COUNT:
      break;  // Intentional failure.
  }
  NOTREACHED() << "Cannot determine filling source";
  return FillingSource::PASSWORD_FALLBACKS;
}

}  // namespace

ManualFillingControllerImpl::~ManualFillingControllerImpl() = default;

// static
base::WeakPtr<ManualFillingController> ManualFillingController::GetOrCreate(
    content::WebContents* contents) {
  ManualFillingControllerImpl* mf_controller =
      ManualFillingControllerImpl::FromWebContents(contents);
  if (!mf_controller) {
    ManualFillingControllerImpl::CreateForWebContents(contents);
    mf_controller = ManualFillingControllerImpl::FromWebContents(contents);
    mf_controller->Initialize();
  }
  return mf_controller->AsWeakPtr();
}

// static
void ManualFillingControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    favicon::FaviconService* favicon_service,
    base::WeakPtr<PasswordAccessoryController> pwd_controller,
    base::WeakPtr<AddressAccessoryController> address_controller,
    base::WeakPtr<CreditCardAccessoryController> cc_controller,
    std::unique_ptr<ManualFillingViewInterface> view) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(pwd_controller);
  DCHECK(address_controller);
  DCHECK(cc_controller);
  DCHECK(view);

  web_contents->SetUserData(
      UserDataKey(),
      // Using `new` to access a non-public constructor.
      base::WrapUnique(new ManualFillingControllerImpl(
          web_contents, favicon_service, std::move(pwd_controller),
          std::move(address_controller), std::move(cc_controller),
          std::move(view))));

  FromWebContents(web_contents)->Initialize();
}

void ManualFillingControllerImpl::OnAutomaticGenerationStatusChanged(
    bool available) {
  DCHECK(view_);
  view_->OnAutomaticGenerationStatusChanged(available);
}

void ManualFillingControllerImpl::RefreshSuggestions(
    const AccessorySheetData& accessory_sheet_data) {
  view_->OnItemsAvailable(accessory_sheet_data);
  UpdateSourceAvailability(GetSourceForTab(accessory_sheet_data),
                           !accessory_sheet_data.user_info_list().empty());
}

void ManualFillingControllerImpl::NotifyFocusedInputChanged(
    autofill::mojom::FocusedFieldType focused_field_type) {
  focused_field_type_ = focused_field_type;

  // Ensure warnings and filling state is updated according to focused field.
  if (cc_controller_)
    cc_controller_->RefreshSuggestions();

  // Whenever the focus changes, reset the accessory.
  if (ShouldShowAccessory())
    view_->SwapSheetWithKeyboard();
  else
    view_->CloseAccessorySheet();

  UpdateVisibility();
}

void ManualFillingControllerImpl::UpdateSourceAvailability(
    FillingSource source,
    bool has_suggestions) {
  if (source == FillingSource::AUTOFILL &&
      !base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory)) {
    // Ignore autofill signals if the feature is disabled.
    return;
  }

  if (has_suggestions == available_sources_.contains(source))
    return;

  if (has_suggestions) {
    available_sources_.insert(source);
    UpdateVisibility();
    return;
  }

  available_sources_.erase(source);
  if (!ShouldShowAccessory())
    UpdateVisibility();
}

void ManualFillingControllerImpl::Hide() {
  view_->Hide();
}

void ManualFillingControllerImpl::OnFillingTriggered(
    AccessoryTabType type,
    const autofill::UserInfo::Field& selection) {
  AccessoryController* controller = GetControllerForTab(type);
  if (!controller)
    return;  // Controller not available anymore.
  controller->OnFillingTriggered(selection);
  view_->SwapSheetWithKeyboard();  // Soft-close the keyboard.
}

void ManualFillingControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) const {
  UMA_HISTOGRAM_ENUMERATION("KeyboardAccessory.AccessoryActionSelected",
                            selected_action, AccessoryAction::COUNT);
  AccessoryController* controller = GetControllerForAction(selected_action);
  if (!controller)
    return;  // Controller not available anymore.
  controller->OnOptionSelected(selected_action);
}

void ManualFillingControllerImpl::GetFavicon(
    int desired_size_in_pixel,
    const std::string& credential_origin,
    IconCallback icon_callback) {
  // credential_origin is only available if the credential has a different
  // origin than the focused frame.
  url::Origin origin = url::Origin::Create(GURL(credential_origin));
  if (origin.opaque() && web_contents_->GetFocusedFrame())
    origin = web_contents_->GetFocusedFrame()->GetLastCommittedOrigin();
  if (origin.opaque()) {
    std::move(icon_callback).Run(gfx::Image());
    return;  // Don't proceed for invalid origins (e.g. due to unfocused frame).
  }

  favicon_service_->GetRawFaviconForPageURL(
      origin.GetURL(),
      {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon,
       favicon_base::IconType::kTouchPrecomposedIcon,
       favicon_base::IconType::kWebManifestIcon},
      desired_size_in_pixel,
      /* fallback_to_host = */ true,
      base::BindOnce(&ManualFillingControllerImpl::OnImageFetched,
                     weak_factory_.GetWeakPtr(), std::move(icon_callback)),
      &favicon_tracker_);
}

gfx::NativeView ManualFillingControllerImpl::container_view() const {
  return web_contents_->GetNativeView();
}

// Returns a weak pointer for this object.
base::WeakPtr<ManualFillingController>
ManualFillingControllerImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ManualFillingControllerImpl::Initialize() {
  DCHECK(FromWebContents(web_contents_)) << "Don't call from constructor!";
  if (address_controller_)
    address_controller_->RefreshSuggestions();
  if (cc_controller_)
    cc_controller_->RefreshSuggestions();
}

ManualFillingControllerImpl::ManualFillingControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      favicon_service_(FaviconServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          ServiceAccessType::EXPLICIT_ACCESS)) {
  if (AddressAccessoryController::AllowedForWebContents(web_contents)) {
    address_controller_ =
        AddressAccessoryController::GetOrCreate(web_contents)->AsWeakPtr();
    DCHECK(address_controller_);
  }
  if (CreditCardAccessoryController::AllowedForWebContents(web_contents)) {
    cc_controller_ =
        CreditCardAccessoryController::GetOrCreate(web_contents)->AsWeakPtr();
    DCHECK(cc_controller_);
  }
}

ManualFillingControllerImpl::ManualFillingControllerImpl(
    content::WebContents* web_contents,
    favicon::FaviconService* favicon_service,
    base::WeakPtr<PasswordAccessoryController> pwd_controller,
    base::WeakPtr<AddressAccessoryController> address_controller,
    base::WeakPtr<CreditCardAccessoryController> cc_controller,
    std::unique_ptr<ManualFillingViewInterface> view)
    : web_contents_(web_contents),
      favicon_service_(favicon_service),
      pwd_controller_for_testing_(std::move(pwd_controller)),
      address_controller_(std::move(address_controller)),
      cc_controller_(std::move(cc_controller)),
      view_(std::move(view)) {}

bool ManualFillingControllerImpl::ShouldShowAccessory() const {
  // If we only provide password fallbacks (== accessory V1), show them for
  // passwords and username fields only.
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory) &&
      !base::FeatureList::IsEnabled(
          autofill::features::kAutofillManualFallbackAndroid)) {
    return focused_field_type_ == FocusedFieldType::kFillablePasswordField ||
           (focused_field_type_ == FocusedFieldType::kFillableUsernameField &&
            available_sources_.contains(FillingSource::PASSWORD_FALLBACKS));
  }
  switch (focused_field_type_) {
    // Always show on password fields to provide management and generation.
    case FocusedFieldType::kFillablePasswordField:
      return true;

    // If there are suggestions, show on usual form fields.
    case FocusedFieldType::kFillableUsernameField:
    case FocusedFieldType::kFillableNonSearchField:
      return !available_sources_.empty();

    // Even if there are suggestions, don't show on search fields and textareas.
    case FocusedFieldType::kFillableSearchField:
    case FocusedFieldType::kFillableTextArea:
      return false;  // TODO(https://crbug.com/965478): true on long-press.

    // Never show if the focused field is not explicitly fillable.
    case FocusedFieldType::kUnfillableElement:
    case FocusedFieldType::kUnknown:
      return false;
  }
  NOTREACHED() << "Unhandled field type " << focused_field_type_;
  return false;
}

void ManualFillingControllerImpl::UpdateVisibility() {
  if (ShouldShowAccessory()) {
    view_->ShowWhenKeyboardIsVisible();
  } else {
    view_->Hide();
  }
}

void ManualFillingControllerImpl::OnImageFetched(
    IconCallback icon_callback,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  favicon_base::FaviconImageResult image_result;
  if (bitmap_result.is_valid()) {
    image_result.image =
        gfx::Image::CreateFrom1xPNGBytes(bitmap_result.bitmap_data);
  }
  std::move(icon_callback).Run(image_result.image);
}

AccessoryController* ManualFillingControllerImpl::GetControllerForTab(
    AccessoryTabType type) {
  switch (type) {
    case AccessoryTabType::ADDRESSES:
      return address_controller_.get();
    case AccessoryTabType::PASSWORDS:
      return GetPasswordController();
    case AccessoryTabType::CREDIT_CARDS:
      return cc_controller_.get();
    case AccessoryTabType::TOUCH_TO_FILL:
    case AccessoryTabType::ALL:
    case AccessoryTabType::COUNT:
      break;  // Intentional failure.
  }
  NOTREACHED() << "Controller not defined for tab: " << static_cast<int>(type);
  return nullptr;
}

AccessoryController* ManualFillingControllerImpl::GetControllerForAction(
    AccessoryAction action) const {
  switch (action) {
    case AccessoryAction::GENERATE_PASSWORD_MANUAL:
    case AccessoryAction::MANAGE_PASSWORDS:
    case AccessoryAction::GENERATE_PASSWORD_AUTOMATIC:
      return GetPasswordController();
    case AccessoryAction::MANAGE_ADDRESSES:
      return address_controller_.get();
    case AccessoryAction::MANAGE_CREDIT_CARDS:
      return cc_controller_.get();
    case AccessoryAction::AUTOFILL_SUGGESTION:
    case AccessoryAction::COUNT:
      break;  // Intentional failure;
  }
  NOTREACHED() << "Controller not defined for action: "
               << static_cast<int>(action);
  return nullptr;
}

PasswordAccessoryController*
ManualFillingControllerImpl::GetPasswordController() const {
  if (pwd_controller_for_testing_)
    return pwd_controller_for_testing_.get();

  return PasswordAccessoryController::AllowedForWebContents(web_contents_)
             ? ChromePasswordManagerClient::FromWebContents(web_contents_)
                   ->GetOrCreatePasswordAccessory()
             : nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ManualFillingControllerImpl)
