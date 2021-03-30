// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/cert_database_ash.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/login/login_state/login_state.h"

#include "chromeos/tpm/tpm_token_info_getter.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util_internal.h"

namespace crosapi {

CertDatabaseAsh::CertDatabaseAsh() {
  DCHECK(chromeos::LoginState::IsInitialized());
  chromeos::LoginState::Get()->AddObserver(this);
}

CertDatabaseAsh::~CertDatabaseAsh() {
  chromeos::LoginState::Get()->RemoveObserver(this);
}

void CertDatabaseAsh::BindReceiver(
    mojo::PendingReceiver<mojom::CertDatabase> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void CertDatabaseAsh::GetCertDatabaseInfo(
    GetCertDatabaseInfoCallback callback) {
  // TODO(crbug.com/1146430): For now Lacros-Chrome will initialize certificate
  // database only in session. Revisit later to decide what to do on the login
  // screen.
  if (!chromeos::LoginState::Get()->IsUserLoggedIn()) {
    LOG(ERROR) << "Not implemented";
    std::move(callback).Run(nullptr);
    return;
  }

  // If this is the first attempt to load the TPM, begin the async load.
  if (!is_tpm_token_ready_.has_value()) {
    WaitForTpmTokenReady(std::move(callback));
    return;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();

  // If user is not available or the TPM was previously attempted to be loaded,
  // and failed, don't retry, just return an empty result that indicates error.
  if (!user || !is_tpm_token_ready_.value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Otherwise, if the TPM was already loaded previously, let the
  // caller know.
  // TODO(crbug.com/1146430) For now Lacros-Chrome loads chaps and has access to
  // TPM operations only for affiliated users, because it gives access to
  // system token. Find a way to give unaffiliated users access only to user TPM
  // token.
  mojom::GetCertDatabaseInfoResultPtr result =
      mojom::GetCertDatabaseInfoResult::New();
  result->should_load_chaps = user->IsAffiliated();
  result->software_nss_db_path =
      crypto::GetSoftwareNSSDBPath(
          ProfileManager::GetPrimaryUserProfile()->GetPath())
          .value();
  std::move(callback).Run(std::move(result));
}

void CertDatabaseAsh::WaitForTpmTokenReady(
    GetCertDatabaseInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  AccountId account_id =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile)->GetAccountId();

  std::unique_ptr<chromeos::TPMTokenInfoGetter> scoped_token_info_getter =
      chromeos::TPMTokenInfoGetter::CreateForUserToken(
          account_id, chromeos::CryptohomePkcs11Client::Get(),
          base::ThreadTaskRunnerHandle::Get());
  chromeos::TPMTokenInfoGetter* token_info_getter =
      scoped_token_info_getter.get();

  token_info_getter->Start(base::BindOnce(
      &CertDatabaseAsh::OnTpmTokenReady, weak_factory_.GetWeakPtr(),
      std::move(scoped_token_info_getter), std::move(callback)));
}

void CertDatabaseAsh::OnTpmTokenReady(
    std::unique_ptr<chromeos::TPMTokenInfoGetter> token_getter,
    GetCertDatabaseInfoCallback callback,
    base::Optional<user_data_auth::TpmTokenInfo> token_info) {
  is_tpm_token_ready_ = token_info.has_value();

  // Calling the initial method again. Since |is_tpm_token_ready_| is not empty
  // this time, it will return some result via mojo.
  GetCertDatabaseInfo(std::move(callback));
}

void CertDatabaseAsh::LoggedInStateChanged() {
  // Cached result is valid only within one session and should be reset on
  // sign out. Currently it is not necessary to reset it on sign in, but doesn't
  // hurt.
  is_tpm_token_ready_.reset();
}

}  // namespace crosapi
