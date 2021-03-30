// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_service_chromeos.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/userdataauth/cryptohome_pkcs11_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/tpm_token_info_getter.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "net/cert/nss_cert_database_chromeos.h"

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
// 4) Ask CryptohomePkcs11Client which TPM slot id corresponds to this profile.
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
    base::Optional<user_data_auth::TpmTokenInfo> token_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (token_info.has_value() && token_info->slot() != -1) {
    DVLOG(1) << "Got TPM slot for " << username_hash << ": "
             << token_info->slot();
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&crypto::InitializeTPMForChromeOSUser,
                                  username_hash, token_info->slot()));
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
          account_id, chromeos::CryptohomePkcs11Client::Get(),
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

// Used to convert a callback that takes a net::NSSCertDatabase to one that
// takes a net::NSSCertDatabaseChromeOS.
void CallWithNSSCertDatabase(
    base::OnceCallback<void(net::NSSCertDatabase*)> callback,
    net::NSSCertDatabaseChromeOS* db) {
  std::move(callback).Run(db);
}

}  // namespace

// Creates and manages a NSSCertDatabaseChromeOS.  Created on the UI thread, but
// all other calls are made on the IO thread.
class NssServiceChromeOS::NSSCertDatabaseChromeOSManager {
 public:
  typedef base::OnceCallback<void(net::NSSCertDatabaseChromeOS*)>
      GetNSSCertDatabaseCallback;

  NSSCertDatabaseChromeOSManager(std::string username_hash,
                                 bool user_is_affiliated)
      : username_hash_(std::move(username_hash)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (user_is_affiliated) {
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &NSSCertDatabaseChromeOSManager::EnableNSSSystemKeySlot,
              weak_ptr_factory_.GetWeakPtr()));
    }
  }

  NSSCertDatabaseChromeOSManager(const NSSCertDatabaseChromeOSManager&) =
      delete;
  NSSCertDatabaseChromeOSManager& operator=(
      const NSSCertDatabaseChromeOSManager&) = delete;

  ~NSSCertDatabaseChromeOSManager() { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

  net::NSSCertDatabaseChromeOS* GetNSSCertDatabaseChromeOS(
      NSSCertDatabaseChromeOSManager::GetNSSCertDatabaseCallback callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    if (nss_cert_database_)
      return nss_cert_database_.get();

    ready_callback_list_.AddUnsafe(std::move(callback));

    // If database creation has already started, nothing else to do.
    if (database_creation_started_)
      return nullptr;

    // Otherwise, start creating the database.
    database_creation_started_ = true;
    crypto::ScopedPK11Slot private_slot(crypto::GetPrivateSlotForChromeOSUser(
        username_hash_,
        base::BindOnce(&NSSCertDatabaseChromeOSManager::DidGetPrivateSlot,
                       weak_ptr_factory_.GetWeakPtr())));
    if (private_slot)
      DidGetPrivateSlot(std::move(private_slot));

    return nullptr;
  }

  // Just like GetNSSCertDatabaseChromeOS(), but uses net::NSSCertDatabase
  // instead of net::NSSCertDatabaseChromeOS.
  net::NSSCertDatabase* GetNSSCertDatabase(
      base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
    return GetNSSCertDatabaseChromeOS(
        base::BindOnce(&CallWithNSSCertDatabase, std::move(callback)));
  }

 private:
  using ReadyCallbackList =
      base::OnceCallbackList<GetNSSCertDatabaseCallback::RunType>;

  void EnableNSSSystemKeySlot() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // This should only be called once.
    DCHECK(!pending_system_slot_);

    crypto::ScopedPK11Slot system_slot = crypto::GetSystemNSSKeySlot(
        base::BindOnce(&NSSCertDatabaseChromeOSManager::SetSystemSlotOfDB,
                       weak_ptr_factory_.GetWeakPtr()));
    if (system_slot)
      SetSystemSlotOfDB(std::move(system_slot));
  }

  void SetSystemSlotOfDB(crypto::ScopedPK11Slot system_slot) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK(system_slot);

    pending_system_slot_ = std::move(system_slot);

    base::RepeatingCallback<void(net::NSSCertDatabaseChromeOS*)> callback =
        base::BindRepeating(&NSSCertDatabaseChromeOSManager::SetSystemSlot,
                            weak_ptr_factory_.GetWeakPtr());

    net::NSSCertDatabaseChromeOS* db = GetNSSCertDatabaseChromeOS(callback);
    if (db)
      SetSystemSlot(db);
  }

  void SetSystemSlot(net::NSSCertDatabaseChromeOS* db) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    db->SetSystemSlot(std::move(pending_system_slot_));
  }

  void DidGetPrivateSlot(crypto::ScopedPK11Slot private_slot) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    nss_cert_database_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
        crypto::GetPublicSlotForChromeOSUser(username_hash_),
        std::move(private_slot));

    ready_callback_list_.Notify(nss_cert_database_.get());
  }

  const std::string username_hash_;
  bool database_creation_started_ = false;

  crypto::ScopedPK11Slot pending_system_slot_;

  std::unique_ptr<net::NSSCertDatabaseChromeOS> nss_cert_database_;
  ReadyCallbackList ready_callback_list_;

  base::WeakPtrFactory<NSSCertDatabaseChromeOSManager> weak_ptr_factory_{this};
};

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

  DCHECK(!(username_hash.empty() && user_is_affiliated));

  nss_cert_database_manager_ = std::make_unique<NSSCertDatabaseChromeOSManager>(
      std::move(username_hash), user_is_affiliated);
}

NssServiceChromeOS::~NssServiceChromeOS() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->DeleteSoon(
      FROM_HERE, std::move(nss_cert_database_manager_));
}

NssCertDatabaseGetter
NssServiceChromeOS::CreateNSSCertDatabaseGetterForIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::BindOnce(&NSSCertDatabaseChromeOSManager::GetNSSCertDatabase,
                        base::Unretained(nss_cert_database_manager_.get()));
}
