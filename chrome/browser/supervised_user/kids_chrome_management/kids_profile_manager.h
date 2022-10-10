// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_PROFILE_MANAGER_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_PROFILE_MANAGER_H_

#include "base/strings/string_piece.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
#include "components/prefs/pref_service.h"

// A helper utility to manage the Profile properties consistently.
class KidsProfileManager {
 public:
  // An invididual property which can be read and written.
  class Property {
   public:
    Property() = delete;
    Property(KidsProfileManager* manager, base::StringPiece property_path);
    void Clear();
    void Set(base::StringPiece value);
    void Set(bool value);
    bool GetBool() const;

   private:
    KidsProfileManager* manager_;
    base::StringPiece property_path_;
  };

  // Typically, a set of properties related to a specific custodian (primary or
  // secondary).
  class Custodian {
   public:
    Custodian() = delete;
    Custodian(KidsProfileManager* manager,
              base::StringPiece name_property_path,
              base::StringPiece email_property_path,
              base::StringPiece gaiaID_property_path,
              base::StringPiece profileURL_property_path,
              base::StringPiece imageURL_property_path);
    void Clear();
    void Update(const kids_chrome_management::FamilyMember& family_member);

   private:
    Property name_;
    Property email_;
    Property gaiaID_;
    Property profileURL_;
    Property imageURL_;
  };

  KidsProfileManager() = delete;
  KidsProfileManager(PrefService& pref_service,

                     Profile& profile);
  void UpdateChildAccountStatus(bool is_child_account);
  bool IsChildAccountStatusKnown() const;
  void SetFirstCustodian(kids_chrome_management::FamilyMember member);
  void SetSecondCustodian(kids_chrome_management::FamilyMember member);

 private:
  Custodian primary_custodian_;
  Custodian secondary_custodian_;
  Property supervised_user_id_;
  Property child_account_status_known_;

  PrefService& pref_service_;
  Profile& profile_;  // TODO(b/252793687): Remove once child status can be
                      // controlled in code and tests via identity manager.
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_PROFILE_MANAGER_H_
