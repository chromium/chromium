// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/address_accessory_controller_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/preferences/autofill/settings_launcher_helper.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/plus_addresses/all_plus_addresses_bottom_sheet_controller.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/plus_addresses/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Defines which types to load from the Personal data manager and add as field
// to the address sheet. Order matters.
constexpr FieldType kTypesToInclude[] = {
    FieldType::NAME_FULL,
    FieldType::COMPANY_NAME,
    FieldType::ADDRESS_HOME_LINE1,
    FieldType::ADDRESS_HOME_LINE2,
    FieldType::ADDRESS_HOME_ZIP,
    FieldType::ADDRESS_HOME_CITY,
    FieldType::ADDRESS_HOME_STATE,
    FieldType::ADDRESS_HOME_COUNTRY,
    FieldType::PHONE_HOME_WHOLE_NUMBER,
    FieldType::EMAIL_ADDRESS,
};

void AddProfileInfoAsSelectableField(UserInfo* info,
                                     const AutofillProfile* profile,
                                     FieldType type) {
  std::u16string field = profile->GetRawInfo(type);
  if (type == FieldType::NAME_MIDDLE && field.empty()) {
    field = profile->GetRawInfo(FieldType::NAME_MIDDLE_INITIAL);
  }
  info->add_field(AccessorySheetField(
      /*display_text=*/field, /*text_to_fill=*/field,
      /*a11y_description=*/field, /*id=*/std::string(), /*is_obfuscated=*/false,
      /*selectable=*/true));
}

UserInfo TranslateProfile(const AutofillProfile* profile) {
  UserInfo info;
  for (FieldType field_type : kTypesToInclude) {
    AddProfileInfoAsSelectableField(&info, profile, field_type);
  }
  return info;
}

std::vector<UserInfo> UserInfosForProfiles(
    const std::vector<const AutofillProfile*>& profiles) {
  std::vector<UserInfo> infos(profiles.size());
  base::ranges::transform(profiles, infos.begin(), TranslateProfile);
  return infos;
}

std::vector<FooterCommand> CreateManageAddressesFooter() {
  std::vector<FooterCommand> commands = {FooterCommand(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SHEET_ALL_ADDRESSES_LINK),
      AccessoryAction::MANAGE_ADDRESSES)};
  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled)) {
    commands.emplace_back(FooterCommand(
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_CREATE_NEW_PLUS_ADDRESSES_LINK_ANDROID),
        AccessoryAction::CREATE_PLUS_ADDRESS));
    commands.emplace_back(
        FooterCommand(l10n_util::GetStringUTF16(
                          IDS_PLUS_ADDRESS_SELECT_PLUS_ADDRESS_LINK_ANDROID),
                      AccessoryAction::SELECT_PLUS_ADDRESS));
  }
  return commands;
}

}  // namespace

AddressAccessoryControllerImpl::~AddressAccessoryControllerImpl() {
  if (personal_data_manager_)
    personal_data_manager_->RemoveObserver(this);
}

// static
AddressAccessoryController* AddressAccessoryController::GetOrCreate(
    content::WebContents* web_contents) {
  AddressAccessoryControllerImpl::CreateForWebContents(web_contents);
  return AddressAccessoryControllerImpl::FromWebContents(web_contents);
}

void AddressAccessoryControllerImpl::RegisterFillingSourceObserver(
    FillingSourceObserver observer) {
  source_observer_ = std::move(observer);
}

std::optional<autofill::AccessorySheetData>
AddressAccessoryControllerImpl::GetSheetData() const {
  if (!personal_data_manager_) {
    return std::nullopt;
  }
  std::vector<const AutofillProfile*> profiles =
      personal_data_manager_->address_data_manager().GetProfilesToSuggest();
  std::u16string title_or_empty_message;
  if (profiles.empty()) {
    title_or_empty_message =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SHEET_EMPTY_MESSAGE);
  }
  // TODO: crbug.com/327838324 - Populate the plus address section.
  return autofill::CreateAccessorySheetData(
      autofill::AccessoryTabType::ADDRESSES, title_or_empty_message,
      UserInfosForProfiles(profiles), CreateManageAddressesFooter());
}

void AddressAccessoryControllerImpl::OnFillingTriggered(
    FieldGlobalId focused_field_id,
    const AccessorySheetField& selection) {
  FillValueIntoField(focused_field_id, selection.display_text());
}

void AddressAccessoryControllerImpl::OnPasskeySelected(
    const std::vector<uint8_t>& passkey_id) {
  NOTIMPLEMENTED() << "Passkey support not available in address controller.";
}

void AddressAccessoryControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) {
  switch (selected_action) {
    case AccessoryAction::MANAGE_ADDRESSES:
      autofill::ShowAutofillProfileSettings(&GetWebContents());
      return;
    case AccessoryAction::CREATE_PLUS_ADDRESS: {
      auto* client = ContentAutofillClient::FromWebContents(&GetWebContents());
      client->OfferPlusAddressCreation(
          client->GetLastCommittedPrimaryMainFrameOrigin(),
          base::BindOnce(
              &AddressAccessoryControllerImpl::OnPlusAddressCreated,
              weak_ptr_factory_.GetWeakPtr(),
              GetManualFillingController()->GetLastFocusedFieldId()));
      // TODO: crbug.com/327838324 - Confirm with the UX that the manual filling
      // sheet should be closed after the bottom sheet is closed.
      GetManualFillingController()->Hide();
      return;
    }
    case AccessoryAction::SELECT_PLUS_ADDRESS: {
      if (!all_plus_addresses_bottom_sheet_controller_) {
        all_plus_addresses_bottom_sheet_controller_ = std::make_unique<
            plus_addresses::AllPlusAddressesBottomSheetController>(
            &GetWebContents());
        all_plus_addresses_bottom_sheet_controller_->Show(base::BindOnce(
            &AddressAccessoryControllerImpl::OnPlusAddressSelected,
            weak_ptr_factory_.GetWeakPtr(),
            GetManualFillingController()->GetLastFocusedFieldId()));
      }
      return;
    }
    default:
      NOTREACHED_NORETURN()
          << "Unhandled selected action: " << static_cast<int>(selected_action);
  }
}

void AddressAccessoryControllerImpl::OnToggleChanged(
    AccessoryAction toggled_action,
    bool enabled) {
  NOTREACHED_NORETURN() << "Unhandled toggled action: "
                        << static_cast<int>(toggled_action);
}

void AddressAccessoryControllerImpl::RefreshSuggestions() {
  TRACE_EVENT0("passwords",
               "AddressAccessoryControllerImpl::RefreshSuggestions");
  if (!personal_data_manager_) {
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForBrowserContext(
            GetWebContents().GetBrowserContext());
    personal_data_manager_->AddObserver(this);
  }
  CHECK(source_observer_);
  source_observer_.Run(this, IsFillingSourceAvailable(
                                 personal_data_manager_ &&
                                 !personal_data_manager_->address_data_manager()
                                      .GetProfilesToSuggest()
                                      .empty()));
}

base::WeakPtr<AddressAccessoryController>
AddressAccessoryControllerImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AddressAccessoryControllerImpl::OnPersonalDataChanged() {
  RefreshSuggestions();
}

// static
void AddressAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(mf_controller);

  web_contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new AddressAccessoryControllerImpl(
                                web_contents, std::move(mf_controller))));
}

AddressAccessoryControllerImpl::AddressAccessoryControllerImpl(
    content::WebContents* web_contents)
    : AddressAccessoryControllerImpl(web_contents, nullptr) {}

// Additional creation functions in unit tests only:
AddressAccessoryControllerImpl::AddressAccessoryControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller)
    : content::WebContentsUserData<AddressAccessoryControllerImpl>(
          *web_contents),
      mf_controller_(std::move(mf_controller)),
      personal_data_manager_(nullptr) {}

void AddressAccessoryControllerImpl::OnPlusAddressCreated(
    FieldGlobalId focused_field_id,
    const std::string& plus_address) {
  FillValueIntoField(focused_field_id, base::UTF8ToUTF16(plus_address));
}

void AddressAccessoryControllerImpl::OnPlusAddressSelected(
    FieldGlobalId focused_field_id,
    base::optional_ref<const std::string> plus_address) {
  if (plus_address) {
    FillValueIntoField(focused_field_id,
                       base::UTF8ToUTF16(plus_address.value()));
  }
  all_plus_addresses_bottom_sheet_controller_.reset();
}

void AddressAccessoryControllerImpl::FillValueIntoField(
    FieldGlobalId focused_field_id,
    const std::u16string& value) {
  // Since the data we fill is scoped to the profile and not to a frame, we can
  // fill the focused frame - we basically behave like a keyboard here.
  content::RenderFrameHost* rfh = GetWebContents().GetFocusedFrame();
  if (!rfh) {
    return;
  }
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!driver) {
    return;
  }
  driver->browser_events().ApplyFieldAction(mojom::FieldActionType::kReplaceAll,
                                            mojom::ActionPersistence::kFill,
                                            focused_field_id, value);
}

base::WeakPtr<ManualFillingController>
AddressAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(&GetWebContents());
  DCHECK(mf_controller_);
  return mf_controller_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AddressAccessoryControllerImpl);

}  // namespace autofill
