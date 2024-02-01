// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_UNIFIED_PASSWORD_MANAGER_PROTO_UTILS_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_UNIFIED_PASSWORD_MANAGER_PROTO_UTILS_H_

#include <vector>

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

class ListPasswordsResult;
class PasswordWithLocalData;
class ListAffiliatedPasswordsResult;
class ListPasswordsWithUiInfoResult;

// Returns PasswordWithLocalData based on given `password_form`.
PasswordWithLocalData PasswordWithLocalDataFromPassword(
    const PasswordForm& password_form);

// Returns a PasswordForm for a given `password` with local, chrome-specific
// data.
PasswordForm PasswordFromProtoWithLocalData(
    const PasswordWithLocalData& password);

// Converts the `list_result` to PasswordForms and returns them in a vector.
// `is_account_store` sets proper `in_store` for all PasswordForms.
std::vector<PasswordForm> PasswordVectorFromListResult(
    const ListPasswordsResult& list_result,
    IsAccountStore is_account_store);
std::vector<PasswordForm> PasswordVectorFromListResult(
    const ListAffiliatedPasswordsResult& list_result,
    IsAccountStore is_account_store);
std::vector<PasswordForm> PasswordVectorFromListResult(
    const ListPasswordsWithUiInfoResult& list_result,
    IsAccountStore is_account_store);

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_UNIFIED_PASSWORD_MANAGER_PROTO_UTILS_H_
