// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_edit_delegate_settings_impl.h"

#include "base/stl_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "components/password_manager/core/browser/password_store.h"

PasswordEditDelegateSettingsImpl::PasswordEditDelegateSettingsImpl(
    Profile* profile,
    base::span<const std::unique_ptr<autofill::PasswordForm>> forms_to_change,
    std::vector<base::string16> existing_usernames)
    : profile_(profile), existing_usernames_(std::move(existing_usernames)) {
  DCHECK(!forms_to_change.empty());

  // Deep copy is needed because the forms need to be persisted  and owned by
  // the delegate after the PasswordManagerPresenter stops existing.
  forms_to_change_.reserve(forms_to_change.size());
  for (const auto& password_form : forms_to_change) {
    forms_to_change_.push_back(
        std::make_unique<autofill::PasswordForm>(*password_form));
  }
}

PasswordEditDelegateSettingsImpl::~PasswordEditDelegateSettingsImpl() = default;

void PasswordEditDelegateSettingsImpl::EditSavedPassword(
    const base::string16& new_username,
    const base::string16& new_password) {
  DCHECK(!new_password.empty()) << "The password is empty.";

  const bool username_changed =
      forms_to_change_[0]->username_value != new_username;

  // In case the username changed, make sure that there exists no other
  // credential with the same signon_realm and username.
  if (username_changed && base::Contains(existing_usernames_, new_username)) {
    // TODO(crbug.com/1002021): We shouldn't fail silently.
    DLOG(ERROR) << "A credential with the same signon_realm and username "
                   "already exists.";
    return;
  }

  EditSavedPasswords(profile_, forms_to_change_, new_username, new_password);
}
