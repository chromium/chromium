// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/identity_manager_ash.h"

#include "base/functional/callback.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/crosapi/mojom/identity_manager.mojom.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/gfx/image/image.h"

namespace crosapi {

namespace {

signin::IdentityManager* GetIdentityManager(const std::string& gaia_id) {
  user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user)
    return nullptr;

  Profile* const profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return nullptr;

  return IdentityManagerFactory::GetInstance()->GetForProfileIfExists(profile);
}

AccountInfo GetAccountInfo(const std::string& gaia_id) {
  signin::IdentityManager* identity_manager = GetIdentityManager(gaia_id);
  if (!identity_manager)
    return AccountInfo();
  return identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id);
}
}  // namespace

IdentityManagerAsh::IdentityManagerAsh() = default;

IdentityManagerAsh::~IdentityManagerAsh() = default;

void IdentityManagerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::IdentityManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void IdentityManagerAsh::GetAccountFullName(
    const std::string& gaia_id,
    GetAccountFullNameCallback callback) {
  AccountInfo account_info = GetAccountInfo(gaia_id);
  if (GetAccountInfo(gaia_id).IsEmpty()) {
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run(account_info.full_name);
}

void IdentityManagerAsh::GetAccountImage(const std::string& gaia_id,
                                         GetAccountImageCallback callback) {
  AccountInfo account_info = GetAccountInfo(gaia_id);
  if (account_info.IsEmpty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  std::move(callback).Run(account_info.account_image.AsImageSkia());
}

void IdentityManagerAsh::GetAccountEmail(const std::string& gaia_id,
                                         GetAccountEmailCallback callback) {
  AccountInfo account_info = GetAccountInfo(gaia_id);
  if (GetAccountInfo(gaia_id).IsEmpty()) {
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run(account_info.email);
}

void IdentityManagerAsh::HasAccountWithPersistentError(
    const std::string& gaia_id,
    HasAccountWithPersistentErrorCallback callback) {
  signin::IdentityManager* identity_manager = GetIdentityManager(gaia_id);
  AccountInfo account_info = GetAccountInfo(gaia_id);
  if (!identity_manager || account_info.IsEmpty()) {
    std::move(callback).Run(false);
    return;
  }
  GoogleServiceAuthError error =
      identity_manager->GetErrorStateOfRefreshTokenForAccount(
          account_info.account_id);
  std::move(callback).Run(error.IsPersistentError());
}

}  // namespace crosapi
