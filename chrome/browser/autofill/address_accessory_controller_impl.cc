// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/address_accessory_controller_impl.h"

#include <algorithm>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/preferences/preferences_launcher.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_utils.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Defines which types to load from the Personal data manager and add as field
// to the address sheet. Order matters.
constexpr ServerFieldType kTypesToInclude[] = {
    ServerFieldType::NAME_FULL,
    ServerFieldType::COMPANY_NAME,
    ServerFieldType::ADDRESS_HOME_LINE1,
    ServerFieldType::ADDRESS_HOME_LINE2,
    ServerFieldType::ADDRESS_HOME_ZIP,
    ServerFieldType::ADDRESS_HOME_CITY,
    ServerFieldType::ADDRESS_HOME_STATE,
    ServerFieldType::ADDRESS_HOME_COUNTRY,
    ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
    ServerFieldType::EMAIL_ADDRESS,
};

void AddProfileInfoAsSelectableField(UserInfo* info,
                                     const AutofillProfile* profile,
                                     ServerFieldType type) {
  base::string16 field = profile->GetRawInfo(type);
  if (type == ServerFieldType::NAME_MIDDLE && field.empty()) {
    field = profile->GetRawInfo(ServerFieldType::NAME_MIDDLE_INITIAL);
  }
  info->add_field(UserInfo::Field(field, field, /*is_password=*/false,
                                  /*selectable=*/true));
}

UserInfo TranslateProfile(const AutofillProfile* profile) {
  UserInfo info;
  for (ServerFieldType server_field_type : kTypesToInclude) {
    AddProfileInfoAsSelectableField(&info, profile, server_field_type);
  }
  return info;
}

std::vector<UserInfo> UserInfosForProfiles(
    const std::vector<AutofillProfile*>& profiles) {
  std::vector<UserInfo> infos(profiles.size());
  std::transform(profiles.begin(), profiles.end(), infos.begin(),
                 TranslateProfile);
  return infos;
}

std::vector<FooterCommand> CreateManageAddressesFooter() {
  return {FooterCommand(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SHEET_ALL_ADDRESSES_LINK),
      AccessoryAction::MANAGE_ADDRESSES)};
}

}  // namespace

AddressAccessoryControllerImpl::~AddressAccessoryControllerImpl() {
  if (personal_data_manager_)
    personal_data_manager_->RemoveObserver(this);
}

// static
bool AddressAccessoryController::AllowedForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  if (vr::VrTabHelper::IsInVr(web_contents)) {
    return false;  // TODO(crbug.com/902305): Re-Enable if possible.
  }
  return base::FeatureList::IsEnabled(
      autofill::features::kAutofillKeyboardAccessory);
}

// static
AddressAccessoryController* AddressAccessoryController::GetOrCreate(
    content::WebContents* web_contents) {
  DCHECK(AddressAccessoryController::AllowedForWebContents(web_contents));

  AddressAccessoryControllerImpl::CreateForWebContents(web_contents);
  return AddressAccessoryControllerImpl::FromWebContents(web_contents);
}

// static
AddressAccessoryController* AddressAccessoryController::GetIfExisting(
    content::WebContents* web_contents) {
  return AddressAccessoryControllerImpl::FromWebContents(web_contents);
}

void AddressAccessoryControllerImpl::OnFillingTriggered(
    const UserInfo::Field& selection) {
  // Since the data we fill is scoped to the profile and not to a frame, we can
  // fill the focused frame - we basically behave like a keyboard here.
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents_->GetFocusedFrame());
  if (!driver)
    return;
  driver->RendererShouldFillFieldWithValue(selection.display_text());
}

void AddressAccessoryControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) {
  if (selected_action == AccessoryAction::MANAGE_ADDRESSES) {
    chrome::android::PreferencesLauncher::ShowAutofillProfileSettings(
        web_contents_);
    return;
  }
  NOTREACHED() << "Unhandled selected action: "
               << static_cast<int>(selected_action);
}

void AddressAccessoryControllerImpl::RefreshSuggestions() {
  std::vector<AutofillProfile*> profiles = GetProfiles();
  base::string16 title_or_empty_message;
  if (profiles.empty())
    title_or_empty_message =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SHEET_EMPTY_MESSAGE);
  GetManualFillingController()->RefreshSuggestions(
      autofill::CreateAccessorySheetData(
          autofill::AccessoryTabType::ADDRESSES, title_or_empty_message,
          UserInfosForProfiles(profiles), CreateManageAddressesFooter()));
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
    : web_contents_(web_contents),
      mf_controller_(std::move(mf_controller)),
      personal_data_manager_(nullptr) {}

std::vector<AutofillProfile*> AddressAccessoryControllerImpl::GetProfiles() {
  if (!personal_data_manager_) {
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
    personal_data_manager_->AddObserver(this);
  }
  return personal_data_manager_->GetProfilesToSuggest();
}

base::WeakPtr<ManualFillingController>
AddressAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(web_contents_);
  DCHECK(mf_controller_);
  return mf_controller_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AddressAccessoryControllerImpl)

}  // namespace autofill
