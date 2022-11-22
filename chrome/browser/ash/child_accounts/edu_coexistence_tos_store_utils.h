// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_EDU_COEXISTENCE_TOS_STORE_UTILS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_EDU_COEXISTENCE_TOS_STORE_UTILS_H_

#include <string>
#include <vector>

class Profile;

namespace ash {
namespace edu_coexistence {

// The first google3 cl number that is sent through a policy which is mapped
// to `prefs::kEduCoexistenceToSVersion`. All version numbers sent
// will be greater than or equal to `kMinTOSVersionNumber`.
extern const char kMinTOSVersionNumber[];

// Used to store the gaia id and corresponding accepted terms of
// service version number. The user is the child account but the parent is the
// one who accepts the terms of service.
struct UserConsentInfo {
  UserConsentInfo(const std::string& gaia_id, const std::string& version);
  std::string edu_account_gaia_id;
  std::string edu_coexistence_tos_version;
};

// Returns true if `lhs_version` as a version int is less than
// the version int of `rhs_version`.
// If any of the version strings is invalid, returns false.
bool IsConsentVersionLessThan(const std::string& lhs_version,
                              const std::string& rhs_version);

// Used by EduCoexistenceLoginHandler to insert/update the stored
// UserConsentInfo when the user completes adding their secondary edu account.
// If the account already exists in user's pref, then its accepted tos will be
// updated. Otherwise, a new entry will be created.
// The pref that is used to store the UserConsentInfo is defined in:
// `prefs::kEduCoexistenceToSAcceptedVersion`
// Unlike `SetUserConsentInfoListForProfile` this doesn't overwrite the entire
// stored UserConsentInfo list; it instead updates it.
void UpdateAcceptedToSVersionPref(Profile* profile,
                                  const UserConsentInfo& user_consent_info);

// Overwrites the stored UserConsentInfo to be |user_consent_info_list|.
void SetUserConsentInfoListForProfile(
    Profile* profile,
    const std::vector<UserConsentInfo>& user_consent_info_list);

// Returns the list of UserConsentInfo stored in
// `prefs::kEduCoexistenceToSAcceptedVersion`.
std::vector<UserConsentInfo> GetUserConsentInfoListForProfile(Profile* profile);

// |profile| is the Primary user profile which is the family link user.
std::string GetAcceptedToSVersion(Profile* profile,
                                  const std::string& secondary_edu_gaia_id);

}  // namespace edu_coexistence
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_EDU_COEXISTENCE_TOS_STORE_UTILS_H_
