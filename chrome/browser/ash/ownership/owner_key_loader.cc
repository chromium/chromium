// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ownership/owner_key_loader.h"

#include "base/check_is_test.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/nss_cert_database.h"

namespace ash {

namespace {

// Max number of attempts to generate a new owner key.
constexpr int kMaxGenerateAttempts = 5;

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
    base::OnceCallback<void(scoped_refptr<ownership::PrivateKey>)>
        ui_thread_callback,
    net::NSSCertDatabase* database) {
  // TODO(davidben): FindPrivateKeyInSlot internally checks for a null slot if
  // needbe. The null check should be in the caller rather than internally in
  // the OwnerKeyUtil implementation. The tests currently get a null
  // private_slot and expect the mock OwnerKeyUtil to still be called.
  scoped_refptr<ownership::PrivateKey> private_key =
      base::MakeRefCounted<ownership::PrivateKey>(
          owner_key_util->FindPrivateKeyInSlot(
              public_key->data(), database->GetPrivateSlot().get()));
  if (!private_key->key()) {
    private_key = base::MakeRefCounted<ownership::PrivateKey>(
        owner_key_util->FindPrivateKeyInSlot(public_key->data(),
                                             database->GetPublicSlot().get()));
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(ui_thread_callback), private_key));
}

void GenerateNewOwnerKeyOnWorkerThread(
    scoped_refptr<ownership::OwnerKeyUtil> owner_key_util,
    base::OnceCallback<void(scoped_refptr<ownership::PublicKey>,
                            scoped_refptr<ownership::PrivateKey>)>
        ui_thread_callback,
    net::NSSCertDatabase* nss_db) {
  crypto::ScopedSECKEYPrivateKey sec_priv_key =
      owner_key_util->GenerateKeyPair(nss_db->GetPublicSlot().get());
  if (!sec_priv_key) {
    LOG(ERROR) << "Failed to generete owner key";
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

void PostOnWorkerThreadWithCertDb(
    base::OnceCallback<void(net::NSSCertDatabase*)> worker_task,
    net::NSSCertDatabase* nss_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // TODO(eseckler): It seems loading the key is important for the UsersPrivate
  // extension API to work correctly during startup, which is why we cannot
  // currently use the BEST_EFFORT TaskPriority here.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(std::move(worker_task), nss_db));
}

void GetCertDbAndPostOnWorkerThreadOnIO(
    NssCertDatabaseGetter nss_getter,
    base::OnceCallback<void(net::NSSCertDatabase*)> worker_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Running |nss_getter| may either return a non-null pointer
  // synchronously or invoke the given callback asynchronously with a non-null
  // pointer. |callback_split| is used here to handle both cases.
  auto callback_split = base::SplitOnceCallback(
      base::BindOnce(&PostOnWorkerThreadWithCertDb, std::move(worker_task)));

  net::NSSCertDatabase* database =
      std::move(nss_getter).Run(std::move(callback_split.first));
  if (database)
    std::move(callback_split.second).Run(database);
}

void GetCertDbAndPostOnWorkerThread(
    Profile* profile,
    base::OnceCallback<void(net::NSSCertDatabase*)> worker_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&GetCertDbAndPostOnWorkerThreadOnIO,
                                NssServiceFactory::GetForContext(profile)
                                    ->CreateNSSCertDatabaseGetterForIOThread(),
                                std::move(worker_task)));
}

}  // namespace

OwnerKeyLoader::OwnerKeyLoader(
    Profile* profile,
    DeviceSettingsService* device_settings_service,
    scoped_refptr<ownership::OwnerKeyUtil> owner_key_util,
    KeypairCallback callback)
    : profile_(profile),
      device_settings_service_(device_settings_service),
      owner_key_util_(std::move(owner_key_util)),
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

  if (!device_settings_service_) {
    CHECK_IS_TEST();
    std::move(callback_).Run(/*public_key=*/nullptr, /*private_key=*/nullptr);
    // `this` might be deleted here.
    return;
  }

  // device_settings_service_ indicates that the current user should become the
  // owner, generate a new owner key pair for them.
  if (device_settings_service_->GetWillEstablishConsumerOwnership()) {
    LOG(WARNING) << "Establishing consumer ownership.";
    GetCertDbAndPostOnWorkerThread(
        profile_,
        base::BindOnce(&GenerateNewOwnerKeyOnWorkerThread, owner_key_util_,
                       base::BindOnce(&OwnerKeyLoader::OnNewKeyGenerated,
                                      weak_factory_.GetWeakPtr())));
    return;
  }

  // Otherwise it might be the owner or not, start with loading the public key.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&LoadPublicKeyOnlyOnWorkerThread, owner_key_util_,
                     base::BindOnce(&OwnerKeyLoader::OnPublicKeyLoaded,
                                    weak_factory_.GetWeakPtr())));
}

void OwnerKeyLoader::OnPublicKeyLoaded(
    scoped_refptr<ownership::PublicKey> public_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!public_key || public_key->is_empty()) {
    // This should not happen. For the very first user that doesn't have the
    // public key yet, device_settings_service_ should indicate that. For other
    // users, they should either have the public key or session_manager should
    // recover it from the policies or session_manager should initiate powerwash
    // if both policies and the public key were lost.
    LOG(ERROR) << "Failed to load public key.";
    std::move(callback_).Run(
        /*public_key=*/nullptr, /*private_key=*/nullptr);
    // `this` might be deleted here.
    return;
  }
  public_key_ = public_key;

  // Now check whether the current user has access to the private key associated
  // with the public key.
  GetCertDbAndPostOnWorkerThread(
      profile_, base::BindOnce(
                    &LoadPrivateKeyOnWorkerThread, owner_key_util_, public_key_,
                    base::BindOnce(&OwnerKeyLoader::OnPrivateKeyLoaded,
                                   weak_factory_.GetWeakPtr())));
}

void OwnerKeyLoader::OnPrivateKeyLoaded(
    scoped_refptr<ownership::PrivateKey> private_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (private_key && private_key->key()) {
    // Success: both keys were loaded, the current user is the owner.
    std::move(callback_).Run(public_key_, private_key);
    // `this` might be deleted here.
    return;
  }

  // Private key failed to load. Maybe the current user is not the owner. Or
  // the private key was lost. Check the policies to make the decision.
  if (device_settings_service_->policy_data()) {
    MaybeRegenerateLostKey(device_settings_service_->policy_data());
    return;
  }
  // If policy data is not available yet, try waiting for it. The assumption is
  // that it can be loaded before this class finishes its work. The public key
  // is usually required to load the policies, but device_settings_service_ also
  // independently loads it for itself.
  device_settings_service_->GetPolicyDataAsync(base::BindOnce(
      &OwnerKeyLoader::OnPolicyDataReady, weak_factory_.GetWeakPtr()));
}

void OwnerKeyLoader::OnNewKeyGenerated(
    scoped_refptr<ownership::PublicKey> public_key,
    scoped_refptr<ownership::PrivateKey> private_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (private_key && private_key->key()) {
    LOG(WARNING) << "New owner key pair was generated.";
    std::move(callback_).Run(public_key, private_key);
    // `this` might be deleted here.
    return;
  }

  if (++generate_attempt_counter_ <= kMaxGenerateAttempts) {
    // Key generation is not expected to fail, but it is too important to simply
    // give up. Retry up to `kMaxGenerateAttempts` times if needed.
    GetCertDbAndPostOnWorkerThread(
        profile_,
        base::BindOnce(&GenerateNewOwnerKeyOnWorkerThread, owner_key_util_,
                       base::BindOnce(&OwnerKeyLoader::OnNewKeyGenerated,
                                      weak_factory_.GetWeakPtr())));
    return;
  }

  LOG(ERROR) << "Failed to generate new owner key.";
  // Return at least the public key, if it was loaded. If Chrome is taking
  // ownership for the first time, it should be null. If recovering from a lost
  // private key, it should be not null.
  std::move(callback_).Run(public_key_, nullptr);
  // `this` might be deleted here.
}

// This method is needed just to convert non-const pointer to a const one.
// OnceCallback struggles to do it itself.
void OwnerKeyLoader::OnPolicyDataReady(
    enterprise_management::PolicyData* policy_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MaybeRegenerateLostKey(policy_data);
}

void OwnerKeyLoader::MaybeRegenerateLostKey(
    const enterprise_management::PolicyData* policy_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If the policy says that the current user is the owner, generate a new key
  // pair for them. Also, in theory ChromeOS is allowed to lose the policies and
  // recover, so be prepared for them to still be empty.
  if (policy_data && policy_data->has_username() &&
      (policy_data->username() == profile_->GetProfileUserName())) {
    LOG(WARNING) << "The owner key was lost. Generating a new one.";
    GetCertDbAndPostOnWorkerThread(
        profile_,
        base::BindOnce(&GenerateNewOwnerKeyOnWorkerThread, owner_key_util_,
                       base::BindOnce(&OwnerKeyLoader::OnNewKeyGenerated,
                                      weak_factory_.GetWeakPtr())));
    return;
  }

  // The user doesn't seem to be the owner, return just the public key.
  std::move(callback_).Run(public_key_, /*private_key=*/nullptr);
  // `this` might be deleted here.
}

}  // namespace ash
