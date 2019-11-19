// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_EDIT_DELEGATE_SETTINGS_IMPL_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_EDIT_DELEGATE_SETTINGS_IMPL_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/android/password_edit_delegate.h"
#include "chrome/browser/android/password_editing_bridge.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

class Profile;

// This is created and destroyed together with the bridge and holds
// all the information about the password form that was loaded and edited in the
// PasswordEntryEditor.
class PasswordEditDelegateSettingsImpl : public PasswordEditDelegate {
 public:
  // |forms_to_change| contains all the password forms that share a sort key
  // with the form that will be edited. |existing_usernames| belong to other
  // saved credentials for the same site and are used to check if the edited
  // username conflicts with any previously existing ones.
  PasswordEditDelegateSettingsImpl(
      Profile* profile,
      base::span<const std::unique_ptr<autofill::PasswordForm>> forms_to_change,
      std::vector<base::string16> existing_usernames);
  ~PasswordEditDelegateSettingsImpl() override;

  void EditSavedPassword(const base::string16& new_username,
                         const base::string16& new_password) override;

 private:
  Profile* profile_ = nullptr;
  std::vector<base::string16> existing_usernames_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> forms_to_change_;
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_EDIT_DELEGATE_SETTINGS_IMPL_H_
