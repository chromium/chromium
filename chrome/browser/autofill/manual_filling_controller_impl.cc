// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_controller_impl.h"

#include <numeric>
#include <utility>

#include "base/callback.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "chrome/browser/autofill/address_accessory_controller.h"
#include "chrome/browser/autofill/credit_card_accessory_controller.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "chrome/browser/password_manager/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_accessory_metrics_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessoryTabType;
using autofill::AddressAccessoryController;
using autofill::CreditCardAccessoryController;
using autofill::mojom::FocusedFieldType;

using FillingSource = ManualFillingController::FillingSource;

namespace {

constexpr auto kAllowedFillingSources = base::MakeFixedFlatSet<FillingSource>(
    {FillingSource::PASSWORD_FALLBACKS, FillingSource::CREDIT_CARD_FALLBACKS,
     FillingSource::ADDRESS_FALLBACKS});

FillingSource GetSourceForTabType(const AccessorySheetData& accessory_sheet) {
  switch (accessory_sheet.get_sheet_type()) {
    case AccessoryTabType::PASSWORDS:
      return FillingSource::PASSWORD_FALLBACKS;
    case AccessoryTabType::CREDIT_CARDS:
      return FillingSource::CREDIT_CARD_FALLBACKS;
    case AccessoryTabType::ADDRESSES:
      return FillingSource::ADDRESS_FALLBACKS;
    case AccessoryTabType::OBSOLETE_TOUCH_TO_FILL:
    case AccessoryTabType::ALL:
    case AccessoryTabType::COUNT:
      NOTREACHED() << "Cannot determine filling source";
      return FillingSource::PASSWORD_FALLBACKS;
  }
}

// This method describes whether an action is sufficiently relevant to show a
// fallback sheet for their own sake in the accessory V1. As of this writing,
// they filter mainly the "Manage" entry points since they are only useful if
// users stored data already.
bool IsRelevantActionForVisibility(AccessoryAction action) {
  switch (action) {
    case AccessoryAction::MANAGE_CREDIT_CARDS:
    case AccessoryAction::MANAGE_ADDRESSES:
      // These options don't provide merit on their own. If the user has no data
      // in the fallback sheet, "manage" is not likely to be useful. The space
      // the accessory consumes is very hard to justify in this case.
      return false;

    case AccessoryAction::MANAGE_PASSWORDS:
      // Technically, the "Manage" entry point justifies showing the bar if two
      // conditions are met:
      // a) the user has at least one password for *any* site (and looks for it)
      // b) the user wants to sign up and needs to adjust their settings
      // But a) is covered by the presence of USE_OTHER_PASSWORD and b) is
      // covered by
      // * the pwd generation logic showing the bar for new sites, OR
      // * the recovery toggle showing the bar for already encountered sites.
      // Therefore, Manage Passwords is a weak signal that shows the accessory
      // too often and in cases where it doesn't make sense: most notably on
      // non-search inputs that *could be a username* – even without any stored
      // passwords. This applies to almost every form in a field
      return false;

    // The cases below don't exist in fallback sheets as of this writing. But if
    // they ever move there, the sheet has a strong reason to be accessible.
    case AccessoryAction::AUTOFILL_SUGGESTION:
    case AccessoryAction::GENERATE_PASSWORD_AUTOMATIC:
    case AccessoryAction::TOGGLE_SAVE_PASSWORDS:
      return true;

    // These cases are sufficient as a reason for showing the fallback sheet.
    case AccessoryAction::USE_OTHER_PASSWORD:
    case AccessoryAction::GENERATE_PASSWORD_MANUAL:
      return true;

    case AccessoryAction::COUNT:
      NOTREACHED();
  }
  return false;
}

// This method filters which actions are sufficient on their own to justify
// showing the V1 version of the accessory. With V2, that decision is moved into
// each `AccessoryController::GetSheetData` implementation.
// In general, if there is any saved data, we want to show the fallback.
bool HasRelevantSuggestions(const AccessorySheetData& accessory_sheet_data) {
  return !accessory_sheet_data.user_info_list().empty() ||
         !accessory_sheet_data.promo_code_info_list().empty() ||
         accessory_sheet_data.option_toggle().has_value() ||
         base::ranges::any_of(accessory_sheet_data.footer_commands(),
                              &IsRelevantActionForVisibility,
                              &autofill::FooterCommand::accessory_action);
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
  TRACE_EVENT0("passwords", "ManualFillingControllerImpl::RefreshSuggestions");
  view_->OnItemsAvailable(accessory_sheet_data);
  available_sheets_.insert_or_assign(GetSourceForTabType(accessory_sheet_data),
                                     accessory_sheet_data);
  UpdateSourceAvailability(GetSourceForTabType(accessory_sheet_data),
                           HasRelevantSuggestions(accessory_sheet_data));
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
  if (cc_controller_)
    cc_controller_->RefreshSuggestions();

  // Whenever the focus changes, reset the accessory.
  if (ShouldShowAccessory())
    view_->SwapSheetWithKeyboard();
  else
    view_->CloseAccessorySheet();

  UpdateVisibility();
}

void ManualFillingControllerImpl::ShowAccessorySheetTab(
    const autofill::AccessoryTabType& tab_type) {
  if (tab_type == autofill::AccessoryTabType::CREDIT_CARDS) {
    cc_controller_->RefreshSuggestions();
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
    const autofill::AccessorySheetField& selection) {
  AccessoryController* controller = GetControllerForTabType(type);
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

void ManualFillingControllerImpl::RequestAccessorySheet(
    autofill::AccessoryTabType tab_type,
    base::OnceCallback<void(autofill::AccessorySheetData)> callback) {
  // TODO(crbug.com/1169167): Consider to execute this async to reduce jank.
  absl::optional<AccessorySheetData> sheet =
      GetControllerForTabType(tab_type)->GetSheetData();
  // After they were loaded, all currently existing sheet types always return a
  // value and will always result in a called callback.
  // The only case where they are not available is before their first load (so
  // if a user entered a tab but didn't focus any fields yet). In that case, the
  // update is unnecessary since the first focus will push the correct sheet.
  // TODO(crbug.com/1169167): Consider sending a null or default sheet to cover
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
  if (PasswordAccessoryController::AllowedForWebContents(web_contents)) {
    pwd_controller_ = ChromePasswordManagerClient::FromWebContents(web_contents)
                          ->GetOrCreatePasswordAccessory()
                          ->AsWeakPtr();
    DCHECK(pwd_controller_);
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
      this, "ManualFillingCache",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

ManualFillingControllerImpl::ManualFillingControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<PasswordAccessoryController> pwd_controller,
    base::WeakPtr<AddressAccessoryController> address_controller,
    base::WeakPtr<CreditCardAccessoryController> cc_controller,
    std::unique_ptr<ManualFillingViewInterface> view)
    : content::WebContentsUserData<ManualFillingControllerImpl>(*web_contents),
      pwd_controller_(std::move(pwd_controller)),
      address_controller_(std::move(address_controller)),
      cc_controller_(std::move(cc_controller)),
      view_(std::move(view)) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "ManualFillingCache",
      base::SingleThreadTaskRunner::GetCurrentDefault());
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
          autofill::features::kAutofillManualFallbackAndroid) &&
      !base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableManualFallbackForVirtualCards)) {
    return last_focused_field_type_ ==
               FocusedFieldType::kFillablePasswordField ||
           last_focused_field_type_ == FocusedFieldType::kFillableUsernameField;
  }
  switch (last_focused_field_type_) {
    // If there are suggestions, show on usual form fields.
    case FocusedFieldType::kFillablePasswordField:
    case FocusedFieldType::kFillableUsernameField:
    case FocusedFieldType::kFillableNonSearchField:
      return !available_sources_.empty();

    // Fallbacks aren't really useful on search fields but autocomplete entries
    // justify showing the accessory.
    case FocusedFieldType::kFillableSearchField:
      return available_sources_.contains(FillingSource::AUTOFILL);

    // Even if there are suggestions, don't show on textareas.
    case FocusedFieldType::kFillableTextArea:
      return false;  // TODO(https://crbug.com/965478): true on long-press.

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
      if (available_sheets_.contains(source)) {
        // Use a local cached copy if it exists. This will be deprecated with
        // crbug.com/1169167.
        view_->OnItemsAvailable(available_sheets_.find(source)->second);
        continue;
      }
      if (source == FillingSource::AUTOFILL)
        continue;  // Autofill suggestions have no sheet.
      absl::optional<AccessorySheetData> sheet =
          GetControllerForFillingSource(source)->GetSheetData();
      if (sheet.has_value())
        view_->OnItemsAvailable(std::move(sheet.value()));
    }
    view_->Show(ManualFillingViewInterface::WaitForKeyboard(
        last_focused_field_type_ != FocusedFieldType::kUnfillableElement &&
        last_focused_field_type_ != FocusedFieldType::kUnknown));

  } else {
    view_->Hide();
  }
}

void ManualFillingControllerImpl::RegisterObserverForAllowedSources() {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory)) {
    return;  // Observer mechanism only available for the modern accessory.
  }
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
  absl::optional<AccessorySheetData> sheet = source_controller->GetSheetData();
  bool show_filling_source = sheet.has_value() && is_source_available;
  // TODO(crbug.com/1169167): Remove once all sheets pull this information
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
      return cc_controller_.get();
    case AccessoryTabType::OBSOLETE_TOUCH_TO_FILL:
    case AccessoryTabType::ALL:
    case AccessoryTabType::COUNT:
      NOTREACHED() << "Controller not defined for tab: "
                   << static_cast<int>(type);
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
      return pwd_controller_.get();
    case AccessoryAction::MANAGE_ADDRESSES:
      return address_controller_.get();
    case AccessoryAction::MANAGE_CREDIT_CARDS:
      return cc_controller_.get();
    case AccessoryAction::AUTOFILL_SUGGESTION:
    case AccessoryAction::COUNT:
      NOTREACHED() << "Controller not defined for action: "
                   << static_cast<int>(action);
      return nullptr;
  }
}

AccessoryController* ManualFillingControllerImpl::GetControllerForFillingSource(
    const FillingSource& filling_source) const {
  switch (filling_source) {
    case FillingSource::PASSWORD_FALLBACKS:
      return pwd_controller_.get();
    case FillingSource::CREDIT_CARD_FALLBACKS:
      return cc_controller_.get();
    case FillingSource::ADDRESS_FALLBACKS:
      return address_controller_.get();
    case FillingSource::AUTOFILL:
      NOTREACHED() << "Controller not defined for filling source: "
                   << static_cast<int>(filling_source);
      return nullptr;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ManualFillingControllerImpl);
