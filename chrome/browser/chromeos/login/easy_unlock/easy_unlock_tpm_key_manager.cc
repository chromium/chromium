// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <stdint.h>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_key_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"

namespace chromeos {
namespace {

// The modulus length for RSA keys used by easy sign-in.
const int kKeyModulusLength = 2048;

// Relays |GetSystemSlotOnIOThread| callback to |response_task_runner|.
void RunCallbackOnTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& response_task_runner,
    const base::Callback<void(crypto::ScopedPK11Slot)>& callback,
    crypto::ScopedPK11Slot slot) {
  response_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(callback, base::Passed(&slot)));
}

// Gets TPM system slot. Must be called on IO thread.
// The callback wil be relayed to |response_task_runner|.
void GetSystemSlotOnIOThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& response_task_runner,
    const base::Callback<void(crypto::ScopedPK11Slot)>& callback) {
  base::Callback<void(crypto::ScopedPK11Slot)> callback_on_origin_thread =
      base::Bind(&RunCallbackOnTaskRunner, response_task_runner, callback);

  crypto::ScopedPK11Slot system_slot =
      crypto::GetSystemNSSKeySlot(callback_on_origin_thread);
  if (system_slot)
    callback_on_origin_thread.Run(std::move(system_slot));
}

// Relays |EnsureUserTpmInitializedOnIOThread| callback to
// |response_task_runner|, ignoring |slot|.
void RunCallbackWithoutSlotOnTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& response_task_runner,
    const base::Closure& callback,
    crypto::ScopedPK11Slot slot) {
  response_task_runner->PostTask(FROM_HERE, callback);
}

void EnsureUserTPMInitializedOnIOThread(
    const std::string& username_hash,
    const scoped_refptr<base::SingleThreadTaskRunner>& response_task_runner,
    const base::Closure& callback) {
  base::Callback<void(crypto::ScopedPK11Slot)> callback_on_origin_thread =
      base::Bind(&RunCallbackWithoutSlotOnTaskRunner, response_task_runner,
                 callback);

  crypto::ScopedPK11Slot private_slot = crypto::GetPrivateSlotForChromeOSUser(
      username_hash, callback_on_origin_thread);
  if (private_slot)
    callback_on_origin_thread.Run(std::move(private_slot));
}

// Checks if a private RSA key associated with |public_key| can be found in
// |slot|. |slot| must be non-null.
// Must be called on a worker thread.
crypto::ScopedSECKEYPrivateKey GetPrivateKeyOnWorkerThread(
    PK11SlotInfo* slot,
    const std::string& public_key) {
  CHECK(slot);

  const uint8_t* public_key_uint8 =
      reinterpret_cast<const uint8_t*>(public_key.data());
  std::vector<uint8_t> public_key_vector(public_key_uint8,
                                         public_key_uint8 + public_key.size());

  crypto::ScopedSECKEYPrivateKey rsa_key(
      crypto::FindNSSKeyFromPublicKeyInfoInSlot(public_key_vector, slot));
  if (!rsa_key || SECKEY_GetPrivateKeyType(rsa_key.get()) != rsaKey)
    return nullptr;
  return rsa_key;
}

// Signs |data| using a private key associated with |public_key| and stored in
// |slot|. Once the data is signed, callback is run on |response_task_runner|.
// In case of an error, the callback will be passed an empty string.
void SignDataOnWorkerThread(
    crypto::ScopedPK11Slot slot,
    const std::string& public_key,
    const std::string& data,
    const scoped_refptr<base::SingleThreadTaskRunner>& response_task_runner,
    const base::Callback<void(const std::string&)>& callback) {
  crypto::ScopedSECKEYPrivateKey private_key(
      GetPrivateKeyOnWorkerThread(slot.get(), public_key));
  if (!private_key) {
    LOG(ERROR) << "Private key for signing data not found";
    response_task_runner->PostTask(FROM_HERE,
                                   base::BindOnce(callback, std::string()));
    return;
  }

  crypto::ScopedSECItem sign_result(SECITEM_AllocItem(NULL, NULL, 0));
  if (SEC_SignData(sign_result.get(),
                   reinterpret_cast<const unsigned char*>(data.data()),
                   data.size(), private_key.get(),
                   SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION) != SECSuccess) {
    LOG(ERROR) << "Failed to sign data";
    response_task_runner->PostTask(FROM_HERE,
                                   base::BindOnce(callback, std::string()));
    return;
  }

  std::string signature(reinterpret_cast<const char*>(sign_result->data),
                        sign_result->len);
  response_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(callback, signature));
}

// Creates a RSA key pair in |slot|. When done, it runs |callback| with the
// created public key on |response_task_runner|.
// If |public_key| is not empty, a key pair will be created only if the private
// key associated with |public_key| does not exist in |slot|. Otherwise the
// callback will be run with |public_key|.
void CreateTpmKeyPairOnWorkerThread(
    crypto::ScopedPK11Slot slot,
    const std::string& public_key,
    const scoped_refptr<base::SingleThreadTaskRunner>& response_task_runner,
    const base::Callback<void(const std::string&)>& callback) {
  if (!public_key.empty() &&
      GetPrivateKeyOnWorkerThread(slot.get(), public_key)) {
    response_task_runner->PostTask(FROM_HERE,
                                   base::BindOnce(callback, public_key));
    return;
  }

  crypto::ScopedSECKEYPublicKey public_key_obj;
  crypto::ScopedSECKEYPrivateKey private_key_obj;
  if (!crypto::GenerateRSAKeyPairNSS(slot.get(), kKeyModulusLength,
                                     true /* permanent */, &public_key_obj,
                                     &private_key_obj)) {
    LOG(ERROR) << "Failed to create an RSA key.";
    response_task_runner->PostTask(FROM_HERE,
                                   base::BindOnce(callback, std::string()));
    return;
  }

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key_obj.get()));
  if (!public_key_der) {
    LOG(ERROR) << "Failed to export public key.";
    response_task_runner->PostTask(FROM_HERE,
                                   base::BindOnce(callback, std::string()));
    return;
  }

  response_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(callback, std::string(reinterpret_cast<const char*>(
                                               public_key_der->data),
                                           public_key_der->len)));
}

}  // namespace

// static
void EasyUnlockTpmKeyManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kEasyUnlockLocalStateTpmKeys);
}

// static
void EasyUnlockTpmKeyManager::ResetLocalStateForUser(
    const AccountId& account_id) {
  if (!g_browser_process)
    return;
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;

  DictionaryPrefUpdate update(local_state, prefs::kEasyUnlockLocalStateTpmKeys);
  update->RemoveWithoutPathExpansion(account_id.GetUserEmail(), NULL);
}

EasyUnlockTpmKeyManager::EasyUnlockTpmKeyManager(
    const AccountId& account_id,
    const std::string& username_hash,
    PrefService* local_state)
    : account_id_(account_id),
      username_hash_(username_hash),
      local_state_(local_state),
      create_tpm_key_state_(CREATE_TPM_KEY_NOT_STARTED) {}

EasyUnlockTpmKeyManager::~EasyUnlockTpmKeyManager() {}

bool EasyUnlockTpmKeyManager::PrepareTpmKey(bool check_private_key,
                                            const base::Closure& callback) {
  CHECK(account_id_.is_valid());
  CHECK(!username_hash_.empty());

  if (create_tpm_key_state_ == CREATE_TPM_KEY_DONE)
    return true;

  std::string key = GetPublicTpmKey(account_id_);
  if (!check_private_key && !key.empty() &&
      create_tpm_key_state_ == CREATE_TPM_KEY_NOT_STARTED) {
    return true;
  }

  prepare_tpm_key_callbacks_.push_back(callback);

  if (create_tpm_key_state_ == CREATE_TPM_KEY_NOT_STARTED) {
    create_tpm_key_state_ = CREATE_TPM_KEY_WAITING_FOR_USER_SLOT;

    base::Closure on_user_tpm_ready =
        base::Bind(&EasyUnlockTpmKeyManager::OnUserTPMInitialized,
                   get_tpm_slot_weak_ptr_factory_.GetWeakPtr(), key);

    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&EnsureUserTPMInitializedOnIOThread, username_hash_,
                       base::ThreadTaskRunnerHandle::Get(), on_user_tpm_ready));
  }

  return false;
}

bool EasyUnlockTpmKeyManager::StartGetSystemSlotTimeoutMs(size_t timeout_ms) {
  if (StartedCreatingTpmKeys())
    return false;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EasyUnlockTpmKeyManager::OnTpmKeyCreated,
                     get_tpm_slot_weak_ptr_factory_.GetWeakPtr(),
                     std::string()),
      base::TimeDelta::FromMilliseconds(timeout_ms));
  return true;
}

std::string EasyUnlockTpmKeyManager::GetPublicTpmKey(
    const AccountId& account_id) {
  if (!local_state_)
    return std::string();
  const base::DictionaryValue* dict =
      local_state_->GetDictionary(prefs::kEasyUnlockLocalStateTpmKeys);
  std::string key;
  if (dict)
    dict->GetStringWithoutPathExpansion(account_id.GetUserEmail(), &key);
  std::string decoded;
  base::Base64Decode(key, &decoded);
  return decoded;
}

void EasyUnlockTpmKeyManager::SignUsingTpmKey(
    const AccountId& account_id,
    const std::string& data,
    const base::Callback<void(const std::string& data)> callback) {
  const std::string key = GetPublicTpmKey(account_id);
  if (key.empty()) {
    callback.Run(std::string());
    return;
  }

  base::Callback<void(crypto::ScopedPK11Slot)> sign_with_system_slot =
      base::Bind(&EasyUnlockTpmKeyManager::SignDataWithSystemSlot,
                 weak_ptr_factory_.GetWeakPtr(), key, data, callback);

  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(&GetSystemSlotOnIOThread,
                                base::ThreadTaskRunnerHandle::Get(),
                                sign_with_system_slot));
}

bool EasyUnlockTpmKeyManager::StartedCreatingTpmKeys() const {
  return create_tpm_key_state_ == CREATE_TPM_KEY_GOT_SYSTEM_SLOT ||
         create_tpm_key_state_ == CREATE_TPM_KEY_DONE;
}

void EasyUnlockTpmKeyManager::SetKeyInLocalState(const AccountId& account_id,
                                                 const std::string& value) {
  if (!local_state_)
    return;

  std::string encoded;
  base::Base64Encode(value, &encoded);
  DictionaryPrefUpdate update(local_state_,
                              prefs::kEasyUnlockLocalStateTpmKeys);
  update->SetKey(account_id.GetUserEmail(), base::Value(encoded));
}

void EasyUnlockTpmKeyManager::OnUserTPMInitialized(
    const std::string& public_key) {
  create_tpm_key_state_ = CREATE_TPM_KEY_WAITING_FOR_SYSTEM_SLOT;

  base::Callback<void(crypto::ScopedPK11Slot)> create_key_with_system_slot =
      base::Bind(&EasyUnlockTpmKeyManager::CreateKeyInSystemSlot,
                 get_tpm_slot_weak_ptr_factory_.GetWeakPtr(), public_key);

  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(&GetSystemSlotOnIOThread,
                                base::ThreadTaskRunnerHandle::Get(),
                                create_key_with_system_slot));
}

void EasyUnlockTpmKeyManager::CreateKeyInSystemSlot(
    const std::string& public_key,
    crypto::ScopedPK11Slot system_slot) {
  CHECK(system_slot);
  create_tpm_key_state_ = CREATE_TPM_KEY_GOT_SYSTEM_SLOT;

  // If there are any delayed tasks posted using |StartGetSystemSlotTimeoutMs|,
  // this will cancel them.
  // Note that this would cancel other pending |CreateKeyInSystemSlot| tasks,
  // but there should be at most one such task at a time.
  get_tpm_slot_weak_ptr_factory_.InvalidateWeakPtrs();

  // This task interacts with the TPM, hence MayBlock().
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CreateTpmKeyPairOnWorkerThread,
                     base::Passed(&system_slot), public_key,
                     base::ThreadTaskRunnerHandle::Get(),
                     base::Bind(&EasyUnlockTpmKeyManager::OnTpmKeyCreated,
                                weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockTpmKeyManager::SignDataWithSystemSlot(
    const std::string& public_key,
    const std::string& data,
    const base::Callback<void(const std::string& data)> callback,
    crypto::ScopedPK11Slot system_slot) {
  CHECK(system_slot);

  // This task interacts with the TPM, hence MayBlock().
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SignDataOnWorkerThread, base::Passed(&system_slot),
                     public_key, data, base::ThreadTaskRunnerHandle::Get(),
                     base::Bind(&EasyUnlockTpmKeyManager::OnDataSigned,
                                weak_ptr_factory_.GetWeakPtr(), callback)));
}

void EasyUnlockTpmKeyManager::OnTpmKeyCreated(const std::string& public_key) {
  // |OnTpmKeyCreated| is called by a timeout task posted by
  // |StartGetSystemSlotTimeoutMs|. Invalidating the factory will have
  // an effect of canceling any pending |GetSystemSlotOnIOThread| callbacks,
  // as well as other pending timeouts.
  // Note that in the case |OnTpmKeyCreated| was called as a result of
  // |CreateKeyInSystemSlot|, this should have no effect as no weak ptrs from
  // this factory should be in use in this case.
  get_tpm_slot_weak_ptr_factory_.InvalidateWeakPtrs();

  if (!public_key.empty())
    SetKeyInLocalState(account_id_, public_key);

  for (size_t i = 0; i < prepare_tpm_key_callbacks_.size(); ++i) {
    if (!prepare_tpm_key_callbacks_[i].is_null())
      prepare_tpm_key_callbacks_[i].Run();
  }

  prepare_tpm_key_callbacks_.clear();

  // If key creation failed, reset the state machine.
  create_tpm_key_state_ =
      public_key.empty() ? CREATE_TPM_KEY_NOT_STARTED : CREATE_TPM_KEY_DONE;
}

void EasyUnlockTpmKeyManager::OnDataSigned(
    const base::Callback<void(const std::string&)>& callback,
    const std::string& signature) {
  callback.Run(signature);
}

}  // namespace chromeos
