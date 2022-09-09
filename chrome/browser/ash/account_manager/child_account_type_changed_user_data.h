// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_CHILD_ACCOUNT_TYPE_CHANGED_USER_DATA_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_CHILD_ACCOUNT_TYPE_CHANGED_USER_DATA_H_

#include "base/callback_list.h"
#include "base/supports_user_data.h"

class Profile;

namespace ash {

class ChildAccountTypeChangedUserData : public base::SupportsUserData::Data {
 public:
  ChildAccountTypeChangedUserData();
  ~ChildAccountTypeChangedUserData() override;
  ChildAccountTypeChangedUserData(const ChildAccountTypeChangedUserData&) =
      delete;
  ChildAccountTypeChangedUserData& operator=(
      const ChildAccountTypeChangedUserData&) = delete;

  static ChildAccountTypeChangedUserData* GetForProfile(Profile* profile);

  // |value| is true if account type was changed from Regular to Child or from
  // Child to Regular on the session start; false otherwise.
  void SetValue(bool value);
  bool value() const;

  base::CallbackListSubscription RegisterCallback(
      const base::RepeatingCallback<void(bool)>& cb);

 private:
  bool value_ = false;
  base::RepeatingCallbackList<void(bool)> callback_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_CHILD_ACCOUNT_TYPE_CHANGED_USER_DATA_H_
