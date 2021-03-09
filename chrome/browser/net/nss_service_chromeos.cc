// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_service_chromeos.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/tpm_token_info_getter.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"

using content::BrowserThread;

namespace {

// The following four functions are responsible for initializing NSS for each
// profile on ChromeOS Ash, which has a separate NSS database and TPM slot
// per-profile.
//
// Initialization basically follows these steps:
// 1) Get some info from user_manager::UserManager about the User for this
// profile.
// 2) Tell nss_util to initialize the software slot for this profile.
// 3) Wait for the TPM module to be loaded by nss_util if it isn't already.
// 4) Ask CryptohomeClient which TPM slot id corresponds to this profile.
// 5) Tell nss_util to use that slot id on the TPM module.
//
// Some of these steps must happen on the UI thread, others must happen on the
// IO thread:
//               UI thread                              IO Thread
//
//  NssServiceChromeOS::NssServiceChromeOS
//                   |
//  ProfileHelper::Get()->GetUserByProfile()
//                   \---------------------------------------v
//                                                 StartNSSInitOnIOThread
//                                                           |
//                                          crypto::InitializeNSSForChromeOSUser
//                                                           |
//                                                crypto::IsTPMTokenReady
//                                                           |
//                                          StartTPMSlotInitializationOnIOThread
//                   v---------------------------------------/
//     GetTPMInfoForUserOnUIThread
//                   |
// chromeos::TPMTokenInfoGetter::Start
//                   |
//     DidGetTPMInfoForUserOnUIThread
//                   \---------------------------------------v
//                                          crypto::InitializeTPMForChromeOSUser

void DidGetTPMInfoForUserOnUIThread(
    std::unique_ptr<chromeos::TPMTokenInfoGetter> getter,
    const std::string& username_hash,
    base::Optional<chromeos::CryptohomeClient::TpmTokenInfo> token_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (token_info.has_value() && token_info->slot != -1) {
    DVLOG(1) << "Got TPM slot for " << username_hash << ": "
             << token_info->slot;
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&crypto::InitializeTPMForChromeOSUser,
                                  username_hash, token_info->slot));
  } else {
    NOTREACHED() << "TPMTokenInfoGetter reported invalid token.";
  }
}

void GetTPMInfoForUserOnUIThread(const AccountId& account_id,
                                 const std::string& username_hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(1) << "Getting TPM info from cryptohome for "
           << " " << account_id.Serialize() << " " << username_hash;
  std::unique_ptr<chromeos::TPMTokenInfoGetter> scoped_token_info_getter =
      chromeos::TPMTokenInfoGetter::CreateForUserToken(
          account_id, chromeos::CryptohomeClient::Get(),
          base::ThreadTaskRunnerHandle::Get());
  chromeos::TPMTokenInfoGetter* token_info_getter =
      scoped_token_info_getter.get();

  // Bind |token_info_getter| to the callback to ensure it does not go away
  // before TPM token info is fetched.
  token_info_getter->Start(base::BindOnce(&DidGetTPMInfoForUserOnUIThread,
                                          std::move(scoped_token_info_getter),
                                          username_hash));
}

void StartTPMSlotInitializationOnIOThread(const AccountId& account_id,
                                          const std::string& username_hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetTPMInfoForUserOnUIThread, account_id, username_hash));
}

void StartNSSInitOnIOThread(const AccountId& account_id,
                            const std::string& username_hash,
                            const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Starting NSS init for " << account_id.Serialize()
           << "  hash:" << username_hash;

  // Make sure NSS is initialized for the user.
  crypto::InitializeNSSForChromeOSUser(username_hash, path);

  // Check if it's OK to initialize TPM for the user before continuing. This
  // may not be the case if the TPM slot initialization was previously
  // requested for the same user.
  if (!crypto::ShouldInitializeTPMForChromeOSUser(username_hash))
    return;

  crypto::WillInitializeTPMForChromeOSUser(username_hash);

  if (crypto::IsTPMTokenEnabledForNSS()) {
    if (crypto::IsTPMTokenReady(
            base::BindOnce(&StartTPMSlotInitializationOnIOThread, account_id,
                           username_hash))) {
      StartTPMSlotInitializationOnIOThread(account_id, username_hash);
    } else {
      DVLOG(1) << "Waiting for tpm ready ...";
    }
  } else {
    crypto::InitializePrivateSoftwareSlotForChromeOSUser(username_hash);
  }
}

void EnableNSSSystemKeySlotOnIOThread(
    content::ResourceContext* resource_context,
    const std::string& username_hash,
    bool user_is_affiliated) {
  SetNSSCertDatabaseUsernameHash(resource_context, username_hash);
  if (user_is_affiliated)
    EnableNSSSystemKeySlotForResourceContext(resource_context);
}

}  // namespace

NssServiceChromeOS::NssServiceChromeOS(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  // No need to initialize NSS for users with empty username hash:
  // Getters for a user's NSS slots always return a null slot if the user's
  // username hash is empty, even when the NSS is not initialized for the
  // user.
  std::string username_hash;
  bool user_is_affiliated = false;
  if (user && !user->username_hash().empty()) {
    username_hash = user->username_hash();
    DCHECK(!username_hash.empty());
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&StartNSSInitOnIOThread, user->GetAccountId(),
                                  username_hash, profile->GetPath()));

    user_is_affiliated = user->IsAffiliated();
  }

  // We need to make sure that content initializes its own data structures that
  // are associated with each ResourceContext because we might post this
  // object to the IO thread after this function.
  content::BrowserContext::EnsureResourceContextInitialized(profile);

  DCHECK(!(username_hash.empty() && user_is_affiliated));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&EnableNSSSystemKeySlotOnIOThread,
                                profile->GetResourceContext(), username_hash,
                                user_is_affiliated));
}

NssServiceChromeOS::~NssServiceChromeOS() = default;
