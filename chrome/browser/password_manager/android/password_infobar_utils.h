// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_INFOBAR_UTILS_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_INFOBAR_UTILS_H_

#include "components/signin/public/identity_manager/account_info.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager {

std::optional<AccountInfo> GetAccountInfoForPasswordMessages(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager);

std::string GetDisplayableAccountName(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager);

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_INFOBAR_UTILS_H_
