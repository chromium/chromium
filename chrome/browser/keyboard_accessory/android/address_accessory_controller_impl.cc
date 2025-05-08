// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/address_accessory_controller_impl.h"

#include <algorithm>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_utils.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/autofill/autofill_fallback_surface_launcher.h"
#include "chrome/browser/ui/plus_addresses/android/all_plus_addresses_bottom_sheet_controller.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Defines which types to load from the Personal data manager and add as field
// to the address sheet. Order matters.
constexpr std::array<std::pair<FieldType, AccessorySuggestionType>, 10>
    kTypesToInclude = {{
        {FieldType::NAME_FULL, AccessorySuggestionType::kNameFull},
        {FieldType::COMPANY_NAME, AccessorySuggestionType::kCompanyName},
        {FieldType::ADDRESS_HOME_LINE1, AccessorySuggestionType::kAddressLine1},
        {FieldType::ADDRESS_HOME_LINE2, AccessorySuggestionType::kAddressLine2},
        {FieldType::ADDRESS_HOME_ZIP, AccessorySuggestionType::kZip},
        {FieldType::ADDRESS_HOME_CITY, AccessorySuggestionType::kCity},
        {FieldType::ADDRESS_HOME_STATE, AccessorySuggestionType::kState},
        {FieldType::ADDRESS_HOME_COUNTRY, AccessorySuggestionType::kCountry},
        {FieldType::PHONE_HOME_WHOLE_NUMBER,
         AccessorySuggestionType::kPhoneNumber},
        {FieldType::EMAIL_ADDRESS, AccessorySuggestionType::kEmailAddress},
    }};

void AddProfileInfoAsSelectableField(UserInfo* info,
                                     const AutofillProfile* profile,
                                     FieldType field_type,
                                     AccessorySuggestionType suggestion_type) {
  std::u16string field = profile->GetRawInfo(field_type);
  if (field_type == FieldType::NAME_MIDDLE && field.empty()) {
    field = profile->GetRawInfo(FieldType::NAME_MIDDLE_INITIAL);
  }
  info->add_field(AccessorySheetField::Builder()
                      .SetSuggestionType(suggestion_type)
                      .SetDisplayText(std::move(field))
                      .SetSelectable(true)
                      .Build());
}

UserInfo TranslateProfile(const AutofillProfile* profile) {
  UserInfo info;
  for (auto field_info : kTypesToInclude) {
    AddProfileInfoAsSelectableField(&info, profile, field_info.first,
                                    field_info.second);
  }
  return info;
}

std::vector<UserInfo> UserInfosForProfiles(
    const std::vector<const AutofillProfile*>& profiles) {
  std::vector<UserInfo> infos(profiles.size());
  std::ranges::transform(profiles, infos.begin(), TranslateProfile);
  return infos;
}

}  // namespace

AddressAccessoryControllerImpl::~AddressAccessoryControllerImpl() {
  if (plus_profiles_provider_) {
    plus_profiles_provider_->RemoveObserver(this);
  }
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
  base::span<const plus_addresses::PlusProfile> plus_profiles;
  if (plus_profiles_provider_) {
    plus_profiles = plus_profiles_provider_->GetAffiliatedPlusProfiles();
  }
  std::vector<const AutofillProfile*> profiles;
  if (const autofill::AddressDataManager* adm = adm_observation_.GetSource()) {
    profiles = adm->GetProfilesToSuggest();
  }
  std::u16string user_info_title, plus_address_title;
  if (profiles.empty()) {
    auto* client = ContentAutofillClient::FromWebContents(&GetWebContents());
    if (client && !plus_profiles.empty()) {
      const std::u16string elided_url =
          url_formatter::FormatOriginForSecurityDisplay(
              client->GetLastCommittedPrimaryMainFrameOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
      plus_address_title = l10n_util::GetStringFUTF16(
          IDS_PLUS_ADDRESS_FALLBACK_MANUAL_FILLING_SHEET_TITLE, elided_url);
    } else {
      // User info title is not empty if and only if the list of addresses is
      // empty and there're no plus addresses available.
      user_info_title =
          l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SHEET_EMPTY_MESSAGE);
    }
  }
  AccessorySheetData sheet_data = autofill::CreateAccessorySheetData(
      autofill::AccessoryTabType::ADDRESSES, user_info_title,
      plus_address_title, UserInfosForProfiles(profiles),
      CreateManageAddressesFooter());
  for (const plus_addresses::PlusProfile& plus_profile : plus_profiles) {
    sheet_data.add_plus_address_info(
        PlusAddressInfo(plus_profile.facet.canonical_spec(),
                        base::UTF8ToUTF16(*plus_profile.plus_address)));
  }
  return sheet_data;
}

void AddressAccessoryControllerImpl::OnFillingTriggered(
    FieldGlobalId focused_field_id,
    const AccessorySheetField& selection) {
  FillValueIntoField(focused_field_id, selection.display_text());
  if (selection.suggestion_type() == AccessorySuggestionType::kPlusAddress &&
      plus_address_service_) {
    plus_address_service_->DidFillPlusAddress();
    if (autofill::ContentAutofillClient* autofill_client =
            autofill::ContentAutofillClient::FromWebContents(
                &GetWebContents())) {
      autofill_client->TriggerPlusAddressUserPerceptionSurvey(
          plus_addresses::hats::SurveyType::kFilledPlusAddressViaManualFallack);
    }
  }
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
    case AccessoryAction::CREATE_PLUS_ADDRESS_FROM_ADDRESS_SHEET:
      if (auto* client =
              ContentAutofillClient::FromWebContents(&GetWebContents())) {
        client->OfferPlusAddressCreation(
            client->GetLastCommittedPrimaryMainFrameOrigin(),
            /*is_manual_fallback=*/true,
            base::BindOnce(
                &AddressAccessoryControllerImpl::OnPlusAddressCreated,
                weak_ptr_factory_.GetWeakPtr(),
                GetManualFillingController()->GetLastFocusedFieldId()));
        base::RecordAction(base::UserMetricsAction(
            "PlusAddresses."
            "CreateSuggestionOnAddressManualFallbackSelected"));
        GetManualFillingController()->Hide();
      }
      return;
    case AccessoryAction::SELECT_PLUS_ADDRESS_FROM_ADDRESS_SHEET:
      if (!all_plus_addresses_bottom_sheet_controller_) {
        all_plus_addresses_bottom_sheet_controller_ = std::make_unique<
            plus_addresses::AllPlusAddressesBottomSheetController>(
            &GetWebContents());
        all_plus_addresses_bottom_sheet_controller_->Show(base::BindOnce(
            &AddressAccessoryControllerImpl::OnPlusAddressSelected,
            weak_ptr_factory_.GetWeakPtr(),
            GetManualFillingController()->GetLastFocusedFieldId()));
        base::RecordAction(base::UserMetricsAction(
            "PlusAddresses."
            "SelectPlusAddressOptionOnAddressManualFallbackSelected"));
        GetManualFillingController()->Hide();
      }
      return;
    case AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_ADDRESS_SHEET:
      autofill::ShowManagePlusAddressesPage(GetWebContents());
      base::RecordAction(base::UserMetricsAction(
          "PlusAddresses.ManageOptionOnAddressManualFallbackSelected"));
      return;
    default:
      NOTREACHED() << "Unhandled selected action: "
                   << static_cast<int>(selected_action);
  }
}

void AddressAccessoryControllerImpl::OnToggleChanged(
    AccessoryAction toggled_action,
    bool enabled) {
  NOTREACHED() << "Unhandled toggled action: "
               << static_cast<int>(toggled_action);
}

void AddressAccessoryControllerImpl::RegisterPlusProfilesProvider(
    base::WeakPtr<AffiliatedPlusProfilesProvider> provider) {
  plus_profiles_provider_ = provider;
  if (plus_profiles_provider_) {
    plus_profiles_provider_->AddObserver(this);
  }
}

void AddressAccessoryControllerImpl::RefreshSuggestions() {
  TRACE_EVENT0("passwords",
               "AddressAccessoryControllerImpl::RefreshSuggestions");
  if (!adm_observation_.IsObserving()) {
    adm_observation_.Observe(
        &autofill::PersonalDataManagerFactory::GetForBrowserContext(
             GetWebContents().GetBrowserContext())
             ->address_data_manager());
  }
  CHECK(source_observer_);
  const bool address_data_available =
      !adm_observation_.GetSource()->GetProfilesToSuggest().empty();
  const bool plus_profiles_data_available =
      plus_profiles_provider_ &&
      !plus_profiles_provider_->GetAffiliatedPlusProfiles().empty();
  source_observer_.Run(this,
                       IsFillingSourceAvailable(address_data_available ||
                                                plus_profiles_data_available));
}

base::WeakPtr<AddressAccessoryController>
AddressAccessoryControllerImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AddressAccessoryControllerImpl::OnAddressDataChanged() {
  RefreshSuggestions();
}

void AddressAccessoryControllerImpl::OnAffiliatedPlusProfilesFetched() {
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
      plus_address_service_(PlusAddressServiceFactory::GetForBrowserContext(
          GetWebContents().GetBrowserContext())) {}

std::vector<FooterCommand>
AddressAccessoryControllerImpl::CreateManageAddressesFooter() const {
  std::vector<FooterCommand> commands = {FooterCommand(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SHEET_ALL_ADDRESSES_LINK),
      AccessoryAction::MANAGE_ADDRESSES)};
  // Both `ContentAutofillClient and this controller are instances of the
  // `WebContentsUserData`. There's no no well-defined destruction order between
  // two different `WebContentsUserData` objects. That's why
  // `ContentAutofillClient` cannot be stored in a `raw_ptr` member variable
  // like `PlusAddressService`.
  auto* autofill_client =
      autofill::ContentAutofillClient::FromWebContents(&GetWebContents());
  if (!autofill_client || !plus_address_service_) {
    return commands;
  }
  // Offer plus address creation if it's supported for the current user session
  // and if the user doesn't have any plus addresses created for the current
  // domain.
  if (plus_address_service_->IsPlusAddressCreationEnabled(
          autofill_client->GetLastCommittedPrimaryMainFrameOrigin(),
          autofill_client->IsOffTheRecord()) &&
      plus_profiles_provider_ &&
      plus_profiles_provider_->GetAffiliatedPlusProfiles().empty()) {
    commands.emplace_back(FooterCommand(
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_CREATE_NEW_PLUS_ADDRESSES_LINK_ANDROID),
        AccessoryAction::CREATE_PLUS_ADDRESS_FROM_ADDRESS_SHEET));
  }
  // Offer the user to select the plus address manually if plus address filling
  // is supported for the last committed origin and the user has at least 1 plus
  // address.
  if (plus_address_service_->IsPlusAddressFillingEnabled(
          autofill_client->GetLastCommittedPrimaryMainFrameOrigin()) &&
      !plus_address_service_->GetPlusProfiles().empty()) {
    commands.emplace_back(
        FooterCommand(l10n_util::GetStringUTF16(
                          IDS_PLUS_ADDRESS_SELECT_PLUS_ADDRESS_LINK_ANDROID),
                      AccessoryAction::SELECT_PLUS_ADDRESS_FROM_ADDRESS_SHEET));
  }

  if (plus_profiles_provider_ &&
      !plus_profiles_provider_->GetAffiliatedPlusProfiles().empty()) {
    commands.emplace_back(
        FooterCommand(l10n_util::GetStringUTF16(
                          IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_LINK_ANDROID),
                      AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_ADDRESS_SHEET));
  }
  return commands;
}

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
    base::RecordAction(base::UserMetricsAction(
        "PlusAddresses."
        "StandaloneFillSuggestionOnAddressManualFallbackAccepted"));
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
