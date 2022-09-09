// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/child_account_type_changed_user_data.h"

#include "chrome/browser/profiles/profile.h"

namespace ash {

namespace {
const void* const kChildAccountTypeChangedUserDataUserKey =
    &kChildAccountTypeChangedUserDataUserKey;
}  // namespace

ChildAccountTypeChangedUserData::ChildAccountTypeChangedUserData() = default;
ChildAccountTypeChangedUserData::~ChildAccountTypeChangedUserData() = default;

// static
ChildAccountTypeChangedUserData* ChildAccountTypeChangedUserData::GetForProfile(
    Profile* profile) {
  ChildAccountTypeChangedUserData* user_data =
      static_cast<ChildAccountTypeChangedUserData*>(
          profile->GetUserData(kChildAccountTypeChangedUserDataUserKey));

  if (!user_data) {
    profile->SetUserData(kChildAccountTypeChangedUserDataUserKey,
                         std::make_unique<ChildAccountTypeChangedUserData>());
    user_data = static_cast<ChildAccountTypeChangedUserData*>(
        profile->GetUserData(kChildAccountTypeChangedUserDataUserKey));
  }

  return user_data;
}

void ChildAccountTypeChangedUserData::SetValue(bool value) {
  value_ = value;
  callback_list_.Notify(value_);
}

bool ChildAccountTypeChangedUserData::value() const {
  return value_;
}

base::CallbackListSubscription
ChildAccountTypeChangedUserData::RegisterCallback(
    const base::RepeatingCallback<void(bool)>& cb) {
  return callback_list_.Add(cb);
}

}  // namespace ash
