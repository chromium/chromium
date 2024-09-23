// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"

#include <numeric>
#include <optional>
#include <utility>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/android/address_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/android/affiliated_plus_profiles_cache.h"
#include "chrome/browser/keyboard_accessory/android/password_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/android/payment_method_accessory_controller.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/plus_addresses/features.h"
#include "content/public/browser/web_contents.h"

using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessoryTabType;
using autofill::AddressAccessoryController;
using autofill::PaymentMethodAccessoryController;
using autofill::mojom::FocusedFieldType;

using FillingSource = ManualFillingController::FillingSource;

namespace {

constexpr auto kAllowedFillingSources = base::MakeFixedFlatSet<FillingSource>(
    {FillingSource::PASSWORD_FALLBACKS, FillingSource::CREDIT_CARD_FALLBACKS,
     FillingSource::ADDRESS_FALLBACKS});

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
    base::WeakPtr<PaymentMethodAccessoryController> payment_method_controller,
    std::unique_ptr<ManualFillingViewInterface> view) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(pwd_controller);
  DCHECK(address_controller);
  DCHECK(payment_method_controller);
  DCHECK(view);

  web_contents->SetUserData(
      UserDataKey(),
      // Using `new` to access a non-public constructor.
      base::WrapUnique(new ManualFillingControllerImpl(
          web_contents, std::move(pwd_controller),
          std::move(address_controller), std::move(payment_method_controller),
          std::move(view))));

  FromWebContents(web_contents)->Initialize();
}

void ManualFillingControllerImpl::OnAccessoryActionAvailabilityChanged(
    ShouldShowAction shouldShowAction,
    autofill::AccessoryAction action) {
  DCHECK(view_);
  view_->OnAccessoryActionAvailabilityChanged(shouldShowAction, action);
}

void ManualFillingControllerImpl::NotifyFocusedInputChanged(
    autofill::FieldRendererId focused_field_id,
    autofill::mojom::FocusedFieldType focused_field_type) {
  TRACE_EVENT0("passwords",
               "ManualFillingControllerImpl::NotifyFocusedInputChanged");
  autofill::LocalFrameToken frame_token;
  if (content::RenderFrameHost* rfh = GetWebContents().GetFocusedFrame()) {
    frame_token = autofill::LocalFrameToken(rfh->GetFrameToken().value());
  }
  last_focused_field_id_ = {frame_token, focused_field_id};
  last_focused_field_type_ = focused_field_type;

  // Ensure warnings and filling state is updated according to focused field.
  if (payment_method_controller_) {
    payment_method_controller_->RefreshSuggestions();
  }

  // Whenever the focus changes, reset the accessory.
  if (ShouldShowAccessory())
    view_->SwapSheetWithKeyboard();
  else
    view_->CloseAccessorySheet();

  UpdateVisibility();
}

autofill::FieldGlobalId ManualFillingControllerImpl::GetLastFocusedFieldId()
    const {
  return last_focused_field_id_;
}

void ManualFillingControllerImpl::ShowAccessorySheetTab(
    const autofill::AccessoryTabType& tab_type) {
  if (tab_type == autofill::AccessoryTabType::CREDIT_CARDS) {
    payment_method_controller_->RefreshSuggestions();
  } else {
    NOTIMPLEMENTED()
        << "ShowAccessorySheetTab does not support the given TabType yet "
        << tab_type;
  }
  view_->ShowAccessorySheetTab(tab_type);
}

void ManualFillingControllerImpl::UpdateSourceAvailability(
    FillingSource source,
    bool has_suggestions) {
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
    const autofill::AccessorySheetField& selection) {
  AccessoryController* controller = GetControllerForTabType(type);
  if (!controller)
    return;  // Controller not available anymore.
  controller->OnFillingTriggered(last_focused_field_id_, selection);
  view_->SwapSheetWithKeyboard();  // Soft-close the keyboard.
}

void ManualFillingControllerImpl::OnPasskeySelected(
    AccessoryTabType type,
    const std::vector<uint8_t>& passkey_id) {
  AccessoryController* controller = GetControllerForTabType(type);
  if (!controller) {
    return;  // Controller not available anymore.
  }
  controller->OnPasskeySelected(passkey_id);
  view_->Hide();  // Close the sheet since the passkey sheet will be triggered.
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

void ManualFillingControllerImpl::RequestAccessorySheet(
    autofill::AccessoryTabType tab_type,
    base::OnceCallback<void(autofill::AccessorySheetData)> callback) {
  // TODO(crbug.com/40165275): Consider to execute this async to reduce jank.
  std::optional<AccessorySheetData> sheet =
      GetControllerForTabType(tab_type)->GetSheetData();
  // After they were loaded, all currently existing sheet types always return a
  // value and will always result in a called callback.
  // The only case where they are not available is before their first load (so
  // if a user entered a tab but didn't focus any fields yet). In that case, the
  // update is unnecessary since the first focus will push the correct sheet.
  // TODO(crbug.com/40165275): Consider sending a null or default sheet to cover
  // future cases where we can't rely on a sheet always being available.
  if (sheet.has_value()) {
    std::move(callback).Run(sheet.value());
  }
}

gfx::NativeView ManualFillingControllerImpl::container_view() const {
  // While a const_cast is not ideal. The Autofill API uses const in various
  // spots and the content public API doesn't have const accessors. So the const
  // cast is the lesser of two evils.
  return const_cast<content::WebContents&>(GetWebContents()).GetNativeView();
}

// Returns a weak pointer for this object.
base::WeakPtr<ManualFillingController>
ManualFillingControllerImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ManualFillingControllerImpl::Initialize() {
  DCHECK(FromWebContents(&GetWebContents())) << "Don't call from constructor!";
  RegisterObserverForAllowedSources();
  if (address_controller_)
    address_controller_->RefreshSuggestions();
}

ManualFillingControllerImpl::ManualFillingControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ManualFillingControllerImpl>(*web_contents) {
  pwd_controller_ = ChromePasswordManagerClient::FromWebContents(web_contents)
                        ->GetOrCreatePasswordAccessory()
                        ->AsWeakPtr();
  DCHECK(pwd_controller_);

  address_controller_ =
      AddressAccessoryController::GetOrCreate(web_contents)->AsWeakPtr();
  DCHECK(address_controller_);

  payment_method_controller_ =
      PaymentMethodAccessoryController::GetOrCreate(web_contents)->AsWeakPtr();
  DCHECK(payment_method_controller_);

  InitializePlusProfilesCache();

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "ManualFillingCache",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

ManualFillingControllerImpl::ManualFillingControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<PasswordAccessoryController> pwd_controller,
    base::WeakPtr<AddressAccessoryController> address_controller,
    base::WeakPtr<PaymentMethodAccessoryController> payment_method_controller,
    std::unique_ptr<ManualFillingViewInterface> view)
    : content::WebContentsUserData<ManualFillingControllerImpl>(*web_contents),
      pwd_controller_(std::move(pwd_controller)),
      address_controller_(std::move(address_controller)),
      payment_method_controller_(std::move(payment_method_controller)),
      view_(std::move(view)) {
  InitializePlusProfilesCache();

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "ManualFillingCache",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void ManualFillingControllerImpl::InitializePlusProfilesCache() {
  if (!base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled)) {
    return;
  }
  auto* client =
      autofill::ContentAutofillClient::FromWebContents(&GetWebContents());
  auto* service = PlusAddressServiceFactory::GetForBrowserContext(
      GetWebContents().GetBrowserContext());
  if (client && service) {
    plus_profiles_cache_ =
        std::make_unique<AffiliatedPlusProfilesCache>(client, service);
    pwd_controller_->RegisterPlusProfilesProvider(
        plus_profiles_cache_->GetWeakPtr());
    address_controller_->RegisterPlusProfilesProvider(
        plus_profiles_cache_->GetWeakPtr());
  }
}

bool ManualFillingControllerImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  auto* dump = process_memory_dump->CreateAllocatorDump(
      base::StringPrintf("passwords/manual_filling_controller/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this)));
  // TODO: crbug.com/40165275 - Clean up memory usage logging.
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  /*value=*/0);
  return true;
}

bool ManualFillingControllerImpl::ShouldShowAccessory() const {
  switch (last_focused_field_type_) {
    // If there are suggestions, show on usual form fields.
    case FocusedFieldType::kFillablePasswordField:
    case FocusedFieldType::kFillableUsernameField:
    case FocusedFieldType::kFillableWebauthnTaggedField:
    case FocusedFieldType::kFillableNonSearchField:
      return !available_sources_.empty();

    // Fallbacks aren't really useful on search fields but autocomplete entries
    // justify showing the accessory.
    case FocusedFieldType::kFillableSearchField:
      return available_sources_.contains(FillingSource::AUTOFILL);

    // Even if there are suggestions, don't show on textareas.
    case FocusedFieldType::kFillableTextArea:
      return false;  // TODO(crbug.com/40628376): true on long-press.

    // Sometimes autocomplete entries may be set when the focus is on an unknown
    // or unfillable field.
    case FocusedFieldType::kUnfillableElement:
    case FocusedFieldType::kUnknown:
      return available_sources_.contains(FillingSource::AUTOFILL);
  }
}

void ManualFillingControllerImpl::UpdateVisibility() {
  TRACE_EVENT0("passwords", "ManualFillingControllerImpl::UpdateVisibility");
  if (ShouldShowAccessory()) {
    for (const FillingSource& source : available_sources_) {
      if (source == FillingSource::AUTOFILL)
        continue;  // Autofill suggestions have no sheet.
      AccessoryController* controller = GetControllerForFillingSource(source);
      if (!controller) {
        continue;  // Most-likely, the controller was cleaned up already.
      }
      std::optional<AccessorySheetData> sheet = controller->GetSheetData();
      if (sheet.has_value())
        view_->OnItemsAvailable(std::move(sheet.value()));
    }
    if (plus_profiles_cache_) {
      plus_profiles_cache_->FetchAffiliatedPlusProfiles();
    }
    view_->Show(ManualFillingViewInterface::WaitForKeyboard(
        last_focused_field_type_ != FocusedFieldType::kUnfillableElement &&
        last_focused_field_type_ != FocusedFieldType::kUnknown));

  } else {
    if (plus_profiles_cache_) {
      plus_profiles_cache_->ClearCachedPlusProfiles();
    }
    view_->Hide();
  }
}

void ManualFillingControllerImpl::RegisterObserverForAllowedSources() {
  for (FillingSource source : kAllowedFillingSources) {
    AccessoryController* sheet_controller =
        GetControllerForFillingSource(source);
    if (!sheet_controller)
      continue;  // Ignore disallowed sheets.
    sheet_controller->RegisterFillingSourceObserver(base::BindRepeating(
        &ManualFillingControllerImpl::OnSourceAvailabilityChanged,
        weak_factory_.GetWeakPtr(), source));
  }
}

void ManualFillingControllerImpl::OnSourceAvailabilityChanged(
    FillingSource source,
    AccessoryController* source_controller,
    AccessoryController::IsFillingSourceAvailable is_source_available) {
  TRACE_EVENT0("passwords",
               "ManualFillingControllerImpl::OnSourceAvailabilityChanged");
  std::optional<AccessorySheetData> sheet = source_controller->GetSheetData();
  bool show_filling_source = sheet.has_value() && is_source_available;
  // TODO(crbug.com/40165275): Remove once all sheets pull this information
  // instead of waiting to get it pushed.
  view_->OnItemsAvailable(std::move(sheet.value()));
  UpdateSourceAvailability(source, show_filling_source);
}

AccessoryController* ManualFillingControllerImpl::GetControllerForTabType(
    AccessoryTabType type) const {
  switch (type) {
    case AccessoryTabType::ADDRESSES:
      return address_controller_.get();
    case AccessoryTabType::PASSWORDS:
      return pwd_controller_.get();
    case AccessoryTabType::CREDIT_CARDS:
      return payment_method_controller_.get();
    case AccessoryTabType::OBSOLETE_TOUCH_TO_FILL:
    case AccessoryTabType::ALL:
    case AccessoryTabType::COUNT:
      NOTREACHED_IN_MIGRATION()
          << "Controller not defined for tab: " << static_cast<int>(type);
      return nullptr;
  }
}

AccessoryController* ManualFillingControllerImpl::GetControllerForAction(
    AccessoryAction action) const {
  switch (action) {
    case AccessoryAction::GENERATE_PASSWORD_MANUAL:
    case AccessoryAction::MANAGE_PASSWORDS:
    case AccessoryAction::USE_OTHER_PASSWORD:
    case AccessoryAction::GENERATE_PASSWORD_AUTOMATIC:
    case AccessoryAction::TOGGLE_SAVE_PASSWORDS:
    case AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY:
    case AccessoryAction::CROSS_DEVICE_PASSKEY:
    case AccessoryAction::CREATE_PLUS_ADDRESS_FROM_PASSWORD_SHEET:
    case AccessoryAction::SELECT_PLUS_ADDRESS_FROM_PASSWORD_SHEET:
    case AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_PASSWORD_SHEET:
      return pwd_controller_.get();
    case AccessoryAction::MANAGE_ADDRESSES:
    case AccessoryAction::CREATE_PLUS_ADDRESS_FROM_ADDRESS_SHEET:
    case AccessoryAction::SELECT_PLUS_ADDRESS_FROM_ADDRESS_SHEET:
    case AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_ADDRESS_SHEET:
      return address_controller_.get();
    case AccessoryAction::MANAGE_CREDIT_CARDS:
      return payment_method_controller_.get();
    case AccessoryAction::AUTOFILL_SUGGESTION:
    case AccessoryAction::COUNT:
      NOTREACHED_IN_MIGRATION()
          << "Controller not defined for action: " << static_cast<int>(action);
      return nullptr;
  }
}

AccessoryController* ManualFillingControllerImpl::GetControllerForFillingSource(
    const FillingSource& filling_source) const {
  switch (filling_source) {
    case FillingSource::PASSWORD_FALLBACKS:
      return pwd_controller_.get();
    case FillingSource::CREDIT_CARD_FALLBACKS:
      return payment_method_controller_.get();
    case FillingSource::ADDRESS_FALLBACKS:
      return address_controller_.get();
    case FillingSource::AUTOFILL:
      NOTREACHED_IN_MIGRATION() << "Controller not defined for filling source: "
                                << static_cast<int>(filling_source);
      return nullptr;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ManualFillingControllerImpl);
