// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_controller_impl.h"

#include <numeric>
#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "chrome/browser/autofill/address_accessory_controller.h"
#include "chrome/browser/autofill/credit_card_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_accessory_metrics_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/common/password_manager_features.h"
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

ManualFillingControllerImpl::~ManualFillingControllerImpl() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

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
base::WeakPtr<ManualFillingController> ManualFillingController::Get(
    content::WebContents* contents) {
  ManualFillingControllerImpl* mf_controller =
      ManualFillingControllerImpl::FromWebContents(contents);
  return mf_controller ? mf_controller->AsWeakPtr() : nullptr;
}

// static
void ManualFillingControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
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

  web_contents->SetUserData(UserDataKey(),
                            // Using `new` to access a non-public constructor.
                            base::WrapUnique(new ManualFillingControllerImpl(
                                web_contents, std::move(pwd_controller),
                                std::move(address_controller),
                                std::move(cc_controller), std::move(view))));

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
  available_sheets_.insert_or_assign(GetSourceForTab(accessory_sheet_data),
                                     accessory_sheet_data);
  UpdateSourceAvailability(GetSourceForTab(accessory_sheet_data),
                           !accessory_sheet_data.user_info_list().empty());
}

void ManualFillingControllerImpl::NotifyFocusedInputChanged(
    autofill::FieldRendererId focused_field_id,
    autofill::mojom::FocusedFieldType focused_field_type) {
  TRACE_EVENT0("passwords",
               "ManualFillingControllerImpl::NotifyFocusedInputChanged");
  autofill::LocalFrameToken frame_token;
  if (content::RenderFrameHost* rfh = web_contents_->GetFocusedFrame()) {
    frame_token = autofill::LocalFrameToken(rfh->GetFrameToken().value());
  }
  last_focused_field_id_ = {frame_token, focused_field_id};
  last_focused_field_type_ = focused_field_type;

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
  controller->OnFillingTriggered(last_focused_field_id_, selection);
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

void ManualFillingControllerImpl::OnToggleChanged(
    AccessoryAction toggled_action,
    bool enabled) const {
  AccessoryController* controller = GetControllerForAction(toggled_action);
  if (!controller)
    return;  // Controller not available anymore.
  controller->OnToggleChanged(toggled_action, enabled);
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
    : web_contents_(web_contents) {
  if (PasswordAccessoryController::AllowedForWebContents(web_contents_)) {
    pwd_controller_ =
        ChromePasswordManagerClient::FromWebContents(web_contents_)
            ->GetOrCreatePasswordAccessory()
            ->AsWeakPtr();
    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillKeyboardAccessory)) {
      pwd_controller_->RegisterFillingSourceObserver(base::BindRepeating(
          &ManualFillingControllerImpl::OnSourceAvailabilityChanged,
          weak_factory_.GetWeakPtr(), FillingSource::PASSWORD_FALLBACKS));
    }
  }
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

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "ManualFillingCache", base::ThreadTaskRunnerHandle::Get());
}

ManualFillingControllerImpl::ManualFillingControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<PasswordAccessoryController> pwd_controller,
    base::WeakPtr<AddressAccessoryController> address_controller,
    base::WeakPtr<CreditCardAccessoryController> cc_controller,
    std::unique_ptr<ManualFillingViewInterface> view)
    : web_contents_(web_contents),
      pwd_controller_(std::move(pwd_controller)),
      address_controller_(std::move(address_controller)),
      cc_controller_(std::move(cc_controller)),
      view_(std::move(view)) {
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory)) {
    pwd_controller_->RegisterFillingSourceObserver(base::BindRepeating(
        &ManualFillingControllerImpl::OnSourceAvailabilityChanged,
        weak_factory_.GetWeakPtr(), FillingSource::PASSWORD_FALLBACKS));
  }
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "ManualFillingCache", base::ThreadTaskRunnerHandle::Get());
}

bool ManualFillingControllerImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  auto* dump = process_memory_dump->CreateAllocatorDump(
      base::StringPrintf("passwords/manual_filling_controller/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this)));
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  base::trace_event::EstimateMemoryUsage(available_sheets_));
  return true;
}

bool ManualFillingControllerImpl::ShouldShowAccessory() const {
  // If we only provide password fallbacks (== accessory V1), show them for
  // passwords and username fields only.
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory) &&
      !base::FeatureList::IsEnabled(
          autofill::features::kAutofillManualFallbackAndroid)) {
    return last_focused_field_type_ ==
               FocusedFieldType::kFillablePasswordField ||
           (last_focused_field_type_ ==
                FocusedFieldType::kFillableUsernameField &&
            (base::FeatureList::IsEnabled(
                 password_manager::features::kFillingPasswordsFromAnyOrigin) ||
             available_sources_.contains(FillingSource::PASSWORD_FALLBACKS)));
  }
  switch (last_focused_field_type_) {
    // Always show on password fields to provide management and generation.
    case FocusedFieldType::kFillablePasswordField:
      return true;

    // If there are suggestions, show on usual form fields.
    case FocusedFieldType::kFillableUsernameField:
    case FocusedFieldType::kFillableNonSearchField:
      return !available_sources_.empty() ||
             base::FeatureList::IsEnabled(
                 password_manager::features::kFillingPasswordsFromAnyOrigin);

    // Fallbacks aren't really useful on search fields but autocomplete entries
    // justify showing the accessory.
    case FocusedFieldType::kFillableSearchField:
      return available_sources_.contains(FillingSource::AUTOFILL);

    // Even if there are suggestions, don't show on textareas.
    case FocusedFieldType::kFillableTextArea:
      return false;  // TODO(https://crbug.com/965478): true on long-press.

    // Never show if the focused field is not explicitly fillable.
    case FocusedFieldType::kUnfillableElement:
    case FocusedFieldType::kUnknown:
      return false;
  }
  NOTREACHED() << "Unhandled field type " << last_focused_field_type_;
  return false;
}

void ManualFillingControllerImpl::UpdateVisibility() {
  TRACE_EVENT0("passwords", "ManualFillingControllerImpl::UpdateVisibility");
  if (ShouldShowAccessory()) {
    for (const FillingSource& source : available_sources_) {
      if (!available_sheets_.contains(source))
        continue;
      view_->OnItemsAvailable(available_sheets_.find(source)->second);
    }
    view_->ShowWhenKeyboardIsVisible();
  } else {
    view_->Hide();
  }
}

void ManualFillingControllerImpl::OnSourceAvailabilityChanged(
    FillingSource source,
    AccessoryController* source_controller,
    AccessoryController::IsFillingSourceAvailable is_source_available) {
  base::Optional<AccessorySheetData> sheet = source_controller->GetSheetData();
  bool show_filling_source = sheet.has_value() && is_source_available;
  // TODO(crbug.com/1169167): Remove once all sheets pull this information
  // instead of waiting to get it pushed.
  view_->OnItemsAvailable(std::move(sheet.value()));
  UpdateSourceAvailability(source, show_filling_source);
}

AccessoryController* ManualFillingControllerImpl::GetControllerForTab(
    AccessoryTabType type) {
  switch (type) {
    case AccessoryTabType::ADDRESSES:
      return address_controller_.get();
    case AccessoryTabType::PASSWORDS:
      return pwd_controller_.get();
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
    case AccessoryAction::USE_OTHER_PASSWORD:
    case AccessoryAction::GENERATE_PASSWORD_AUTOMATIC:
    case AccessoryAction::TOGGLE_SAVE_PASSWORDS:
      return pwd_controller_.get();
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(ManualFillingControllerImpl)
