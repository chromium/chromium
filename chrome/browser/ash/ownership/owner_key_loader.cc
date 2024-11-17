// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/ownership/owner_key_loader.h"

#include <string>
#include <utility>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/ownership/ownership_histograms.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/nss_cert_database.h"

namespace ash {

// Enable storing a newly created owner key in the private slot.
BASE_FEATURE(kStoreOwnerKeyInPrivateSlot,
             "StoreOwnerKeyInPrivateSlot",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable migration of the owner key from the public to the private slot. This
// experiment represents the second stage of `kStoreOwnerKeyInPrivateSlot` and
// is only respected if kStoreOwnerKeyInPrivateSlot is enabled.
BASE_FEATURE(kMigrateOwnerKeyToPrivateSlot,
             "MigrateOwnerKeyToPrivateSlot",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsStoreOwnerKeyInPrivateSlotEnabled() {
  return base::FeatureList::IsEnabled(kStoreOwnerKeyInPrivateSlot);
}

bool ShouldMigrateOwnerKeyToPrivateSlot() {
  return IsStoreOwnerKeyInPrivateSlotEnabled() &&
         base::FeatureList::IsEnabled(kMigrateOwnerKeyToPrivateSlot);
}

namespace {

// Max number of attempts to generate a new owner key.
constexpr int kMaxGenerateAttempts = 5;

using WorkerTask =
    base::OnceCallback<void(crypto::ScopedPK11Slot /*public_slot*/,
                            crypto::ScopedPK11Slot /*private_slot*/)>;

void LoadPublicKeyOnlyOnWorkerThread(
    scoped_refptr<ownership::OwnerKeyUtil> owner_key_util,
    base::OnceCallback<void(scoped_refptr<ownership::PublicKey>)>
        ui_thread_callback) {
  scoped_refptr<ownership::PublicKey> public_key =
      owner_key_util->ImportPublicKey();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(ui_thread_callback), public_key));
}

void LoadPrivateKeyOnWorkerThread(
    scoped_refptr<ownership::OwnerKeyUtil> owner_key_util,
    scoped_refptr<ownership::PublicKey> public_key,
    base::OnceCallback<void(scoped_refptr<ownership::PrivateKey>,
                            bool found_in_public_slot)> ui_thread_callback,
    crypto::ScopedPK11Slot public_slot,
    crypto::ScopedPK11Slot private_slot) {
  // TODO(davidben): FindPrivateKeyInSlot internally checks for a null slot if
  // needbe. The null check should be in the caller rather than internally in
  // the OwnerKeyUtil implementation. The tests currently get a null
  // private_slot and expect the mock OwnerKeyUtil to still be called.
  scoped_refptr<ownership::PrivateKey> private_key =
      base::MakeRefCounted<ownership::PrivateKey>(
          owner_key_util->FindPrivateKeyInSlot(public_key->data(),
                                               private_slot.get()));
  bool found_in_public_slot = false;

  if (!private_key->key()) {
    private_key = base::MakeRefCounted<ownership::PrivateKey>(
        owner_key_util->FindPrivateKeyInSlot(public_key->data(),
                                             public_slot.get()));
    if (private_key->key()) {
      // If the key is stored in the public slot, it might need to be migrated
      // (depending on the experiment).
      found_in_public_slot = true;
    }
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(ui_thread_callback), private_key,
                                found_in_public_slot));
}

void GenerateNewOwnerKeyOnWorkerThread(
    scoped_refptr<ownership::OwnerKeyUtil> owner_key_util,
    base::OnceCallback<void(scoped_refptr<ownership::PublicKey>,
                            scoped_refptr<ownership::PrivateKey>)>
        ui_thread_callback,
    crypto::ScopedPK11Slot public_slot,
    crypto::ScopedPK11Slot private_slot) {
  crypto::ScopedSECKEYPrivateKey sec_priv_key;
  if (private_slot && IsStoreOwnerKeyInPrivateSlotEnabled()) {
    sec_priv_key = owner_key_util->GenerateKeyPair(private_slot.get());
    RecordOwnerKeyEvent(OwnerKeyEvent::kPrivateSlotKeyGeneration,
                        bool(sec_priv_key));
  } else if (public_slot) {
    sec_priv_key = owner_key_util->GenerateKeyPair(public_slot.get());
    RecordOwnerKeyEvent(OwnerKeyEvent::kPublicSlotKeyGeneration,
                        bool(sec_priv_key));
  }

  if (!sec_priv_key) {
    LOG(ERROR) << "Failed to generate owner key";
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(ui_thread_callback), nullptr, nullptr));
    return;
  }

  crypto::ScopedSECKEYPublicKey sec_pub_key(
      SECKEY_ConvertToPublicKey(sec_priv_key.get()));
  crypto::ScopedSECItem sec_pub_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(sec_pub_key.get()));
  if (!sec_pub_key_der.get()) {
    LOG(ERROR) << "Failed to extract public key";
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(ui_thread_callback), nullptr, nullptr));
    return;
  }

  scoped_refptr<ownership::PublicKey> public_key =
      base::MakeRefCounted<ownership::PublicKey>(
          /*is_persisted=*/false,
          std::vector<uint8_t>(sec_pub_key_der->data,
                               sec_pub_key_der->data + sec_pub_key_der->len));
  scoped_refptr<ownership::PrivateKey> private_key =
      base::MakeRefCounted<ownership::PrivateKey>(std::move(sec_priv_key));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(ui_thread_callback),
                                std::move(public_key), std::move(private_key)));
}

void PostOnWorkerThreadWithCertDb(WorkerTask worker_task,
                                  net::NSSCertDatabase* nss_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK(nss_db);

  // TODO(eseckler): It seems loading the key is important for the UsersPrivate
  // extension API to work correctly during startup, which is why we cannot
  // currently use the BEST_EFFORT TaskPriority here.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(std::move(worker_task), nss_db->GetPublicSlot(),
                     nss_db->GetPrivateSlot()));
}

void GetCertDbAndPostOnWorkerThreadOnIO(NssCertDatabaseGetter nss_getter,
                                        WorkerTask worker_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Running |nss_getter| may either return a non-null pointer
  // synchronously or invoke the given callback asynchronously with a non-null
  // pointer. |callback_split| is used here to handle both cases.
  auto callback_split = base::SplitOnceCallback(
      base::BindOnce(&PostOnWorkerThreadWithCertDb, std::move(worker_task)));

  net::NSSCertDatabase* database =
      std::move(nss_getter).Run(std::move(callback_split.first));
  if (database) {
    std::move(callback_split.second).Run(database);
  }
}

void GetCertDbAndPostOnWorkerThread(Profile* profile, WorkerTask worker_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&GetCertDbAndPostOnWorkerThreadOnIO,
                                NssServiceFactory::GetForContext(profile)
                                    ->CreateNSSCertDatabaseGetterForIOThread(),
                                std::move(worker_task)));
}

inline bool IsKeyPresent(
    const scoped_refptr<ownership::PublicKey>& public_key) {
  return public_key && !public_key->is_empty();
}

inline bool IsKeyPresent(
    const scoped_refptr<ownership::PrivateKey>& public_key) {
  return public_key && public_key->key();
}

inline bool AreKeysPresent(
    const scoped_refptr<ownership::PublicKey>& public_key,
    const scoped_refptr<ownership::PrivateKey>& private_key) {
  return IsKeyPresent(public_key) && IsKeyPresent(private_key);
}

bool UserCanBecomeOwner(const user_manager::User* user) {
  if (!user) {
    return false;
  }
  switch (user->GetType()) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
      return true;
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return false;
  }
}

}  // namespace

OwnerKeyLoader::OwnerKeyLoader(
    Profile* profile,
    DeviceSettingsService* device_settings_service,
    scoped_refptr<ownership::OwnerKeyUtil> owner_key_util,
    bool is_enterprise_managed,
    KeypairCallback callback)
    : profile_(profile),
      device_settings_service_(device_settings_service),
      owner_key_util_(std::move(owner_key_util)),
      is_enterprise_managed_(is_enterprise_managed),
      callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(owner_key_util_);
  DCHECK(callback_);
  if (!device_settings_service_) {
    CHECK_IS_TEST();
  }
}
OwnerKeyLoader::~OwnerKeyLoader() = default;

void OwnerKeyLoader::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_) << "Run() can only be called once.";

  if (g_browser_process && g_browser_process->IsShuttingDown()) {
    return std::move(callback_).Run(/*public_key=*/nullptr,
                                    /*private_key=*/nullptr);
    // `this` might be deleted here.
  }

  if (!device_settings_service_) {
    CHECK_IS_TEST();
    RecordOwnerKeyEvent(OwnerKeyEvent::kDeviceSettingsServiceIsNull,
                        /*success=*/false);
    return std::move(callback_).Run(/*public_key=*/nullptr,
                                    /*private_key=*/nullptr);
    // `this` might be deleted here.
  }

  // Try loading the public key first. Most of the time it should already exist.
  // Use TaskPriority::USER_VISIBLE priority because some user visible features
  // need to know whether the current user is the owner or not (e.g.
  // UsersPrivate API).
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&LoadPublicKeyOnlyOnWorkerThread, owner_key_util_,
                     base::BindOnce(&OwnerKeyLoader::OnPublicKeyLoaded,
                                    weak_factory_.GetWeakPtr())));
}

void OwnerKeyLoader::OnPublicKeyLoaded(
    scoped_refptr<ownership::PublicKey> public_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  public_key_ = std::move(public_key);

  if (is_enterprise_managed_) {
    RecordOwnerKeyEvent(OwnerKeyEvent::kManagedDevice,
                        /*success=*/IsKeyPresent(public_key_));
    // Managed devices don't have private owner keys.
    return std::move(callback_).Run(std::move(public_key_), nullptr);
    // `this` might be deleted here.
  }

  if (IsKeyPresent(public_key_)) {
    // Now check whether the current user has access to the private key
    // associated with the public key.
    return GetCertDbAndPostOnWorkerThread(
        profile_,
        base::BindOnce(&LoadPrivateKeyOnWorkerThread, owner_key_util_,
                       public_key_,
                       base::BindOnce(&OwnerKeyLoader::OnPrivateKeyLoaded,
                                      weak_factory_.GetWeakPtr())));
  }

  // Public key was not found, if the current user is the owner, a new key
  // should be generated.
  MaybeGenerateNewKey();
}

void OwnerKeyLoader::OnPrivateKeyLoaded(
    scoped_refptr<ownership::PrivateKey> private_key,
    bool found_in_public_slot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsKeyPresent(private_key)) {
    RecordOwnerKeyEvent(OwnerKeyEvent::kOwnerHasKeys,
                        /*success=*/AreKeysPresent(public_key_, private_key));
    RecordOwnerKeyEvent(OwnerKeyEvent::kOwnerKeyInPublicSlot,
                        /*success=*/found_in_public_slot);

    if (ShouldMigrateOwnerKeyToPrivateSlot() && found_in_public_slot) {
      // If the key was found in the public slot and the migration is enabled,
      // then replace it by generating a new one in the private slot. The old
      // key will be deleted by OwnerSettingsServiceAsh when the new key is
      // saved as the owner key.
      LOG(WARNING) << "Found owner key in public slot, migrating to "
                      "private slot.";
      old_owner_key_ = private_key->ExtractKey();
      GenerateNewKey();
      RecordOwnerKeyEvent(OwnerKeyEvent::kMigrationToPrivateSlotStarted,
                          /*success=*/true);
      return;
    }

    if (!ShouldMigrateOwnerKeyToPrivateSlot() &&
        !IsStoreOwnerKeyInPrivateSlotEnabled() && !found_in_public_slot) {
      // If all experiments are disabled but the key is in the private slot, it
      // means they were reverted and it's probably better to migrate the owner
      // key back to the public slot.
      LOG(WARNING) << "Found owner key in private slot while the private slot "
                      "experiments are disabled, migrating to public slot.";
      old_owner_key_ = private_key->ExtractKey();
      GenerateNewKey();
      RecordOwnerKeyEvent(OwnerKeyEvent::kMigrationToPublicSlotStarted,
                          /*success=*/true);
      return;
    }

    // Success: both keys were loaded, the current user is the owner.
    return std::move(callback_).Run(std::move(public_key_),
                                    std::move(private_key));
    // `this` might be deleted here.
  }

  // Private key failed to load. Maybe the current user is not the owner and
  // doesn't have access to the key. Or the private key was lost. Make the
  // decision and generate a new key if needed.
  MaybeGenerateNewKey();
}

void OwnerKeyLoader::MaybeGenerateNewKey() {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  if (!UserCanBecomeOwner(user)) {
    RecordOwnerKeyEvent(OwnerKeyEvent::kUserNotAnOwnerBasedOnUserType,
                        /*success=*/IsKeyPresent(public_key_));
    return std::move(callback_).Run(public_key_, nullptr);
  }

  if (user->GetAccountId().GetUserEmail().empty()) {
    RecordOwnerKeyEvent(OwnerKeyEvent::kUserNotAnOwnerBasedOnEmptyUsername,
                        /*success=*/IsKeyPresent(public_key_));
    // This is not expected to happen, regular users should have a valid
    // username.
    return std::move(callback_).Run(public_key_, nullptr);
  }

  // Check device policies. If the owner key was never generated before, the
  // policies will be empty. Also, in theory ChromeOS is allowed to lose the
  // policies and recover, so be prepared for them to still be empty.
  const enterprise_management::PolicyData* policy_data =
      device_settings_service_->policy_data();
  if (policy_data && policy_data->has_username()) {
    // If the policy says that the current user is the owner, generate a new key
    // pair for them.
    if (policy_data->username() == user->GetAccountId().GetUserEmail()) {
      // Expect public key to be present. It's not likely to lose private and
      // public keys simultaneously, they are stored independently.
      RecordOwnerKeyEvent(OwnerKeyEvent::kRegeneratingOwnerKeyBasedOnPolicy,
                          /*success=*/IsKeyPresent(public_key_));
      LOG(WARNING) << "The owner key was lost. Generating a new one.";
      return GenerateNewKey();
    } else {
      RecordOwnerKeyEvent(OwnerKeyEvent::kUserNotAnOwnerBasedOnPolicy,
                          /*success=*/IsKeyPresent(public_key_));
      // The current user is not the owner, just return the public key.
      return std::move(callback_).Run(std::move(public_key_), nullptr);
      // `this` might be deleted here.
    }
  }

  // If the policies are empty, check the local state PrefService.
  std::optional<std::string> owner_email =
      user_manager::UserManager::Get()->GetOwnerEmail();

  if (owner_email.has_value() &&
      owner_email.value() == user->GetAccountId().GetUserEmail()) {
    // This brunch is more likely to be used before device policies are created
    // for the first time, so expect the public key to not be present.
    RecordOwnerKeyEvent(OwnerKeyEvent::kRegeneratingOwnerKeyBasedOnLocalState,
                        /*success=*/!IsKeyPresent(public_key_));
    LOG(WARNING) << "Generating new owner key based on local state data.";
    return GenerateNewKey();
  } else if (owner_email.has_value() &&
             owner_email.value() != user->GetAccountId().GetUserEmail()) {
    RecordOwnerKeyEvent(OwnerKeyEvent::kUserNotAnOwnerBasedOnLocalState,
                        /*success=*/IsKeyPresent(public_key_));
    return std::move(callback_).Run(std::move(public_key_), nullptr);
    // `this` might be deleted here.
  }

  // If everything else is empty, check DeviceSettingsService. It remembers from
  // the login screen whether this is the first user. It's checked after
  // policies because it relies on local state (same as `GetOwnerEmail()`) and
  // is less trustworthy.
  if (device_settings_service_->GetWillEstablishConsumerOwnership()) {
    // This should only happen on the first sign in when there's no previous
    // public key.
    RecordOwnerKeyEvent(OwnerKeyEvent::kEstablishingConsumerOwnership,
                        /*success=*/!IsKeyPresent(public_key_));
    LOG(WARNING) << "Establishing consumer ownership.";
    return GenerateNewKey();
  }

  // If there's no indication from `device_settings_service_` that this is the
  // first user, but also no signs of other users ever taking ownership, take
  // the ownership for the current user. This is not supposed to happen under
  // normal circumstances, but that's what session_manager did before Chrome
  // took over owner key generation. The main problem with this case is that if
  // a device ends up with no owner and multiple users, the next user that signs
  // in will become the owner (instead of the user that was created on the
  // device first). But it's still better to have some owner on the device than
  // none and it's hard to intentionally destroy all the signs of the existing
  // owner to steal the ownership, so it's considered to be a reasonable
  // fallback.
  if (!IsKeyPresent(public_key_)) {
    RecordOwnerKeyEvent(OwnerKeyEvent::kUnsureTakeOwnership,
                        /*success=*/true);
    return GenerateNewKey();
  }

  RecordOwnerKeyEvent(OwnerKeyEvent::kUnsureUserNotAnOwner,
                      /*success=*/IsKeyPresent(public_key_));
  // If the public key was successfully loaded, then some other user probably
  // generated it and they are the owner and hold the private key for it. The
  // current user doesn't seem to be the owner, just return the public key.
  return std::move(callback_).Run(std::move(public_key_), nullptr);
  // `this` might be deleted here.
}

void OwnerKeyLoader::GenerateNewKey() {
  // Ensure owner account id is stored for the next time.
  if (!user_manager::UserManager::Get()->GetOwnerEmail().has_value()) {
    user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (user) {
      user_manager::UserManager::Get()->RecordOwner(user->GetAccountId());
    } else {
      // Recording the owner is mainly useful for the next launch of Chrome,
      // which doesn't happen in most tests. So just allow skipping it if the
      // user was not set up.
      CHECK_IS_TEST();
    }
  }

  GetCertDbAndPostOnWorkerThread(
      profile_,
      base::BindOnce(&GenerateNewOwnerKeyOnWorkerThread, owner_key_util_,
                     base::BindOnce(&OwnerKeyLoader::OnNewKeyGenerated,
                                    weak_factory_.GetWeakPtr())));
}

void OwnerKeyLoader::OnNewKeyGenerated(
    scoped_refptr<ownership::PublicKey> public_key,
    scoped_refptr<ownership::PrivateKey> private_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (AreKeysPresent(public_key, private_key)) {
    RecordOwnerKeyEvent(OwnerKeyEvent::kOwnerKeyGenerated,
                        /*success=*/(generate_attempt_counter_ == 0));

    LOG(WARNING) << "New owner key pair was generated.";
    return std::move(callback_).Run(std::move(public_key),
                                    std::move(private_key));
    // `this` might be deleted here.
  }

  if (++generate_attempt_counter_ <= kMaxGenerateAttempts) {
    // Key generation is not expected to fail, but it is too important to simply
    // give up. Retry up to `kMaxGenerateAttempts` times if needed.
    return GenerateNewKey();
  }

  // This case is not a success in general, but record whether the user will at
  // least get the old public key.
  RecordOwnerKeyEvent(OwnerKeyEvent::kFailedToGenerateOwnerKey,
                      /*success=*/IsKeyPresent(public_key_));
  LOG(ERROR) << "Failed to generate new owner key.";
  // Return at least the public key, if it was loaded. If Chrome is taking
  // ownership for the first time, it should be null. If recovering from a lost
  // private key, it should be not null.
  return std::move(callback_).Run(std::move(public_key_), nullptr);
  // `this` might be deleted here.
}

crypto::ScopedSECKEYPrivateKey OwnerKeyLoader::ExtractOldOwnerKey() {
  return std::move(old_owner_key_);
}

}  // namespace ash
