// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_service.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/crosapi/cert_database_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_pkcs11_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
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
//  NssService::NssService
//                   |
//  ProfileHelper::Get()->GetUserByProfile()
//                   \---------------------------------------v
//                                                 StartNSSInitOnIOThread
//                                                           |
//                                          crypto::InitializeNSSForChromeOSUser
//                                                           |
//                                                crypto::IsTPMTokenEnabled
//                                                           |
//                                          StartTPMSlotInitializationOnIOThread
//                   v---------------------------------------/
//     GetTPMInfoForUserOnUIThread
//                   |
//   ash::TPMTokenInfoGetter::Start
//                   |
//     DidGetTPMInfoForUserOnUIThread
//                   \---------------------------------------v
//                                          crypto::InitializeTPMForChromeOSUser

void DidGetTPMInfoForUserOnUIThread(
    std::unique_ptr<ash::TPMTokenInfoGetter> getter,
    const std::string& username_hash,
    std::optional<user_data_auth::TpmTokenInfo> token_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (token_info.has_value() && token_info->slot() != -1) {
    DVLOG(1) << "Got TPM slot for " << username_hash << ": "
             << token_info->slot();
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&crypto::InitializeTPMForChromeOSUser,
                                  username_hash, token_info->slot()));
  } else {
    NOTREACHED_IN_MIGRATION() << "TPMTokenInfoGetter reported invalid token.";
  }
}

void GetTPMInfoForUserOnUIThread(const AccountId& account_id,
                                 const std::string& username_hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(1) << "Getting TPM info from cryptohome for "
           << " " << account_id.Serialize() << " " << username_hash;
  std::unique_ptr<ash::TPMTokenInfoGetter> scoped_token_info_getter =
      ash::TPMTokenInfoGetter::CreateForUserToken(
          account_id, ash::CryptohomePkcs11Client::Get(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
  ash::TPMTokenInfoGetter* token_info_getter = scoped_token_info_getter.get();

  // Bind |token_info_getter| to the callback to ensure it does not go away
  // before TPM token info is fetched.
  token_info_getter->Start(base::BindOnce(&DidGetTPMInfoForUserOnUIThread,
                                          std::move(scoped_token_info_getter),
                                          username_hash));
}

void StartTPMSlotInitializationOnIOThread(const AccountId& account_id,
                                          const std::string& username_hash,
                                          bool is_tpm_token_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!is_tpm_token_enabled) {
    crypto::InitializePrivateSoftwareSlotForChromeOSUser(username_hash);
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetTPMInfoForUserOnUIThread, account_id, username_hash));
}

void StartNSSInitOnIOThread(const AccountId& account_id,
                            const std::string& username_hash,
                            const base::FilePath& path,
                            bool is_kiosk) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Starting NSS init for " << account_id.Serialize()
           << "  hash:" << username_hash;

  // Make sure NSS is initialized for the user.
  if (is_kiosk) {
    // Kiosk sessions don't have the UI that could result in interactions with
    // the public slot. Kiosk users are also not owner users and can't have
    // the owner key in the public slot. So the public slot is not used in
    // Kiosk sessions and can be replaced by the internal slot. This is done
    // mainly because Chrome sometimes fails to load the public slot and has
    // to crash because of that.
    crypto::InitializeNSSForChromeOSUserWithSlot(
        username_hash, crypto::ScopedPK11Slot(PK11_GetInternalKeySlot()));
  } else {
    crypto::InitializeNSSForChromeOSUser(username_hash, path);
  }

  // Check if it's OK to initialize TPM for the user before continuing. This
  // may not be the case if the TPM slot initialization was previously
  // requested for the same user.
  if (!crypto::ShouldInitializeTPMForChromeOSUser(username_hash))
    return;

  crypto::WillInitializeTPMForChromeOSUser(username_hash);
  crypto::IsTPMTokenEnabled(base::BindOnce(
      &StartTPMSlotInitializationOnIOThread, account_id, username_hash));
}

void NotifyCertsChangedInAshOnUIThread(
    crosapi::mojom::CertDatabaseChangeType change_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->cert_database_ash()
      ->NotifyCertsChangedInAsh(change_type);
}

}  // namespace

// Creates and manages a NSSCertDatabaseChromeOS. Created on the UI thread, but
// all other calls are made on the IO thread.
class NssService::NSSCertDatabaseChromeOSManager
    : public net::NSSCertDatabase::Observer {
 public:
  using GetNSSCertDatabaseCallback =
      base::OnceCallback<void(net::NSSCertDatabase*)>;

  NSSCertDatabaseChromeOSManager(std::string username_hash,
                                 bool enable_system_slot)
      : username_hash_(std::move(username_hash)),
        enable_system_slot_(enable_system_slot) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  NSSCertDatabaseChromeOSManager(const NSSCertDatabaseChromeOSManager&) =
      delete;
  NSSCertDatabaseChromeOSManager& operator=(
      const NSSCertDatabaseChromeOSManager&) = delete;

  ~NSSCertDatabaseChromeOSManager() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (nss_cert_database_) {
      nss_cert_database_->RemoveObserver(this);
    }
  }

  net::NSSCertDatabase* GetNSSCertDatabase(
      GetNSSCertDatabaseCallback callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    if (nss_cert_database_) {
      return nss_cert_database_.get();
    }

    ready_callback_list_.AddUnsafe(std::move(callback));

    if (!database_creation_started_) {
      database_creation_started_ = true;
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&NSSCertDatabaseChromeOSManager::StartDatabaseCreation,
                         weak_ptr_factory_.GetWeakPtr()));
    }

    return nullptr;
  }

  // net::NSSCertDatabase::Observer
  void OnTrustStoreChanged() override {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&NotifyCertsChangedInAshOnUIThread,
                       crosapi::mojom::CertDatabaseChangeType::kTrustStore));
  }
  void OnClientCertStoreChanged() override {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NotifyCertsChangedInAshOnUIThread,
            crosapi::mojom::CertDatabaseChangeType::kClientCertStore));
  }

 private:
  void StartDatabaseCreation() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    crypto::ScopedPK11Slot private_slot(crypto::GetPrivateSlotForChromeOSUser(
        username_hash_,
        base::BindOnce(&NSSCertDatabaseChromeOSManager::DidGetPrivateSlot,
                       weak_ptr_factory_.GetWeakPtr())));
    if (private_slot)
      DidGetPrivateSlot(std::move(private_slot));
  }

  void DidGetPrivateSlot(crypto::ScopedPK11Slot private_slot) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    if (!enable_system_slot_) {
      CreateDatabase(std::move(private_slot),
                     /*system_slot=*/crypto::ScopedPK11Slot());
      return;
    }

    crypto::GetSystemNSSKeySlot(base::BindOnce(
        &NSSCertDatabaseChromeOSManager::CreateDatabase,
        weak_ptr_factory_.GetWeakPtr(), std::move(private_slot)));
  }

  void CreateDatabase(crypto::ScopedPK11Slot private_slot,
                      crypto::ScopedPK11Slot system_slot) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    auto public_slot = crypto::GetPublicSlotForChromeOSUser(username_hash_);

#if BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE)
    if (!public_slot) {
      // This is a "for testing" branch. The code below will intentionally crash
      // when the public slot fails to load. By default prevent this from
      // happening in tests that simply don't properly fake NSS. Consider using
      // FakeNssService if a specific NSS behavior is required in tests.
      public_slot = crypto::ScopedPK11Slot(PK11_GetInternalKeySlot());
    }
#endif

    // TODO(crbug.com/1163303): Remove when the bug is fixed.
    if (!public_slot) {
      Profile* profile = ProfileManager::GetActiveUserProfile();
      CHECK(profile);
      crypto::DiagnosePublicSlotAndCrash(
          crypto::GetSoftwareNSSDBPath(profile->GetPath()));
    }

    nss_cert_database_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
        std::move(public_slot), std::move(private_slot));

    if (system_slot)
      nss_cert_database_->SetSystemSlot(std::move(system_slot));
    nss_cert_database_->AddObserver(this);

    ready_callback_list_.Notify(nss_cert_database_.get());
  }

  const std::string username_hash_;
  bool enable_system_slot_ = false;
  bool database_creation_started_ = false;

  std::unique_ptr<net::NSSCertDatabaseChromeOS> nss_cert_database_;
  base::OnceCallbackList<GetNSSCertDatabaseCallback::RunType>
      ready_callback_list_;

  base::WeakPtrFactory<NSSCertDatabaseChromeOSManager> weak_ptr_factory_{this};
};

NssService::NssService(content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context);

  Profile* profile = Profile::FromBrowserContext(context);
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  // No need to initialize NSS for users with empty username hash:
  // Getters for a user's NSS slots always return a null slot if the user's
  // username hash is empty, even when the NSS is not initialized for the
  // user.
  std::string username_hash;
  bool enable_system_slot = false;
  if (user && !user->username_hash().empty()) {
    username_hash = user->username_hash();
    DCHECK(!username_hash.empty());
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&StartNSSInitOnIOThread, user->GetAccountId(),
                                  username_hash, profile->GetPath(),
                                  chromeos::IsKioskSession()));

    enable_system_slot = user->IsAffiliated();
  }

  DCHECK(!(username_hash.empty() && enable_system_slot));

  nss_cert_database_manager_ = std::make_unique<NSSCertDatabaseChromeOSManager>(
      std::move(username_hash), enable_system_slot);
}

NssService::~NssService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->DeleteSoon(
      FROM_HERE, std::move(nss_cert_database_manager_));
}

NssCertDatabaseGetter NssService::CreateNSSCertDatabaseGetterForIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::BindOnce(&NSSCertDatabaseChromeOSManager::GetNSSCertDatabase,
                        base::Unretained(nss_cert_database_manager_.get()));
}
