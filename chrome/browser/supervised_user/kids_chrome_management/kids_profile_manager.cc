// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_profile_manager.h"

#include <string>

#include "base/strings/string_piece.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace {
using ::base::StringPiece;
using ::kids_chrome_management::FamilyMember;
}  // namespace

KidsProfileManager::KidsProfileManager(PrefService& pref_service,
                                       Profile& profile)
    : primary_custodian_(this,
                         prefs::kSupervisedUserCustodianName,
                         prefs::kSupervisedUserCustodianEmail,
                         prefs::kSupervisedUserCustodianObfuscatedGaiaId,
                         prefs::kSupervisedUserCustodianProfileURL,
                         prefs::kSupervisedUserCustodianProfileImageURL),
      secondary_custodian_(
          this,
          prefs::kSupervisedUserSecondCustodianName,
          prefs::kSupervisedUserSecondCustodianEmail,
          prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
          prefs::kSupervisedUserSecondCustodianProfileURL,
          prefs::kSupervisedUserSecondCustodianProfileImageURL),
      supervised_user_id_(this, prefs::kSupervisedUserId),
      child_account_status_known_(this, prefs::kChildAccountStatusKnown),
      pref_service_(pref_service),
      profile_(profile) {}

KidsProfileManager::~KidsProfileManager() {}

bool KidsProfileManager::IsChildAccount() const {
  return profile_
      ->IsChild();  // TODO(b/252793687): Use AccountInfo.is_child_account ==
                    // Tribool::kTrue once setting child status is possible in
                    // test and remove the direct Profile dependency.
}

void KidsProfileManager::UpdateChildAccountStatus(bool is_child_account) {
  if (IsChildAccount() != is_child_account) {
    if (is_child_account) {
      supervised_user_id_.Set(StringPiece(supervised_users::kChildAccountSUID));
    } else {
      supervised_user_id_.Clear();
      primary_custodian_.Clear();
      secondary_custodian_.Clear();
    }
  }
  child_account_status_known_.Set(true);
}

bool KidsProfileManager::IsChildAccountStatusKnown() const {
  return child_account_status_known_.GetBool();
}

void KidsProfileManager::SetFirstCustodian(FamilyMember member) {
  primary_custodian_.Update(member);
}

void KidsProfileManager::SetSecondCustodian(FamilyMember member) {
  secondary_custodian_.Update(member);
}

KidsProfileManager::Custodian::Custodian(KidsProfileManager* manager,
                                         StringPiece name_property_path,
                                         StringPiece email_property_path,
                                         StringPiece gaiaID_property_path,
                                         StringPiece profileURL_property_path,
                                         StringPiece imageURL_property_path)
    : name_(manager, name_property_path),
      email_(manager, email_property_path),
      gaiaID_(manager, gaiaID_property_path),
      profileURL_(manager, profileURL_property_path),
      imageURL_(manager, imageURL_property_path) {}

KidsProfileManager::Custodian::~Custodian() {}

void KidsProfileManager::Custodian::Clear() {
  name_.Clear();
  email_.Clear();
  gaiaID_.Clear();
  profileURL_.Clear();
  imageURL_.Clear();
}

void KidsProfileManager::Custodian::Update(const FamilyMember& family_member) {
  name_.Set(family_member.profile().display_name());
  email_.Set(family_member.profile().email());
  gaiaID_.Set(family_member.user_id());
  profileURL_.Set(family_member.profile().profile_url());
  imageURL_.Set(family_member.profile().profile_image_url());
}

KidsProfileManager::Property::Property(KidsProfileManager* manager,
                                       StringPiece property_path)
    : manager_(manager), property_path_(property_path) {}

void KidsProfileManager::Property::Clear() {
  manager_->pref_service_->ClearPref(std::string(property_path_));
}

void KidsProfileManager::Property::Set(StringPiece value) {
  manager_->pref_service_->SetString(std::string(property_path_),
                                     std::string(value));
}

void KidsProfileManager::Property::Set(bool value) {
  manager_->pref_service_->SetBoolean(std::string(property_path_), value);
}

bool KidsProfileManager::Property::GetBool() const {
  return manager_->pref_service_->GetBoolean(std::string(property_path_));
}
