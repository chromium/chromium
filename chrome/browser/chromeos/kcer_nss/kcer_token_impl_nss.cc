// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kcer_nss/kcer_token_impl_nss.h"

#include <certdb.h>
#include <pkcs11.h>
#include <stdint.h>
#include <queue>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/kcer_nss/cert_cache_nss.h"
#include "chrome/browser/chromeos/platform_keys/chaps_util.h"
#include "chromeos/components/kcer/kcer_token.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_key_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// General pattern for implementing KcerToken methods:
// * The received callbacks for the results must already be bound to correct
// task runners (see base::BindPostTask), so it's not needed to manually post
// them on any particular tast runner.
// * Each method must advance task queue on completion. This is important
// because before initialization and during other tasks all new tasks are stored
// in the queue. It can only progress when a previous tasks advances the queue
// or a new task arrives.
// * The most convenient way to advance the queue is to call
// BlockQueueGetUnblocker() and attach the result to the original callback. This
// will advance the queue automatically after the callback is called (or even
// discarded).
// * It's also better to keep the task queue blocked during the execution of a
// callback in case of additional requests from it.

namespace kcer::internal {
namespace {

void RunUnblocker(base::ScopedClosureRunner unblocker) {
  unblocker.RunAndReset();
}

// Returns a vector containing bytes from `value` or an empty vector if `value`
// is nullptr.
std::vector<uint8_t> SECItemToBytes(crypto::ScopedSECItem value) {
  return value ? std::vector<uint8_t>(value->data, value->data + value->len)
               : std::vector<uint8_t>();
}

void CleanUpAndDestroyKeys(crypto::ScopedSECKEYPublicKey public_key,
                           crypto::ScopedSECKEYPrivateKey private_key) {
  // Clean up generated keys. PK11_DeleteTokenPrivateKey and
  // PK11_DeleteTokenPublicKey are documented to also destroy the passed
  // SECKEYPublicKey/SECKEYPrivateKey structures.
  PK11_DeleteTokenPrivateKey(/*privKey=*/private_key.release(),
                             /*force=*/false);
  PK11_DeleteTokenPublicKey(/*pubKey=*/public_key.release());
}

void GenerateRsaKeyOnWorkerThread(Token token,
                                  crypto::ScopedPK11Slot slot,
                                  uint32_t modulus_length_bits,
                                  bool hardware_backed,
                                  Kcer::GenerateKeyCallback callback) {
  bool key_gen_success = false;
  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;

  if (hardware_backed) {
    key_gen_success = crypto::GenerateRSAKeyPairNSS(
        slot.get(), modulus_length_bits, /*permanent=*/true, &public_key,
        &private_key);
  } else {
    auto chaps_util = chromeos::platform_keys::ChapsUtil::Create();
    key_gen_success = chaps_util->GenerateSoftwareBackedRSAKey(
        slot.get(), modulus_length_bits, &public_key, &private_key);
  }

  if (!key_gen_success) {
    CleanUpAndDestroyKeys(std::move(public_key), std::move(private_key));
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToGenerateKey));
  }
  DCHECK(public_key && private_key);

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToExportPublicKey));
  }

  Pkcs11Id pkcs11_id(SECItemToBytes(crypto::ScopedSECItem(
      PK11_MakeIDFromPubKey(&public_key->u.rsa.modulus))));
  PublicKeySpki public_key_spki(SECItemToBytes(std::move(public_key_der)));

  return std::move(callback).Run(
      PublicKey(token, std::move(pkcs11_id), std::move(public_key_spki)));
}

void GenerateEcKeyOnWorkerThread(Token token,
                                 crypto::ScopedPK11Slot slot,
                                 EllipticCurve curve,
                                 bool hardware_backed,
                                 Kcer::GenerateKeyCallback callback) {
  bool key_gen_success = false;
  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;

  if (curve != EllipticCurve::kP256) {
    return std::move(callback).Run(base::unexpected(Error::kNotSupported));
  }

  if (hardware_backed) {
    key_gen_success = crypto::GenerateECKeyPairNSS(
        slot.get(), SEC_OID_ANSIX962_EC_PRIME256V1, /*permanent=*/true,
        &public_key, &private_key);
  } else {
    // Shouldn't be needed yet, will be implemented in non-NSS version of Kcer.
    return std::move(callback).Run(base::unexpected(Error::kNotImplemented));
  }

  if (!key_gen_success) {
    CleanUpAndDestroyKeys(std::move(public_key), std::move(private_key));
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToGenerateKey));
  }

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    CleanUpAndDestroyKeys(std::move(public_key), std::move(private_key));
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToExportPublicKey));
  }

  Pkcs11Id pkcs11_id(SECItemToBytes(crypto::ScopedSECItem(
      PK11_MakeIDFromPubKey(&public_key->u.ec.publicValue))));

  return std::move(callback).Run(
      PublicKey(token, std::move(pkcs11_id),
                PublicKeySpki(SECItemToBytes(std::move(public_key_der)))));
}

void ImportKeyOnWorkerThread(Token token,
                             crypto::ScopedPK11Slot slot,
                             Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                             Kcer::ImportKeyCallback callback) {
  crypto::ScopedSECKEYPrivateKey imported_private_key =
      crypto::ImportNSSKeyFromPrivateKeyInfo(slot.get(),
                                             pkcs8_private_key_info_der.value(),
                                             /*permanent=*/true);
  if (!imported_private_key) {
    return std::move(callback).Run(base::unexpected(Error::kFailedToImportKey));
  }

  crypto::ScopedSECKEYPublicKey public_key(
      SECKEY_ConvertToPublicKey(imported_private_key.get()));
  if (!public_key) {
    CleanUpAndDestroyKeys(std::move(public_key),
                          std::move(imported_private_key));
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToExportPublicKey));
  }

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    CleanUpAndDestroyKeys(std::move(public_key),
                          std::move(imported_private_key));
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToEncodePublicKey));
  }

  Pkcs11Id pkcs11_id(
      SECItemToBytes(crypto::MakeNssIdFromPublicKey(public_key.get())));

  return std::move(callback).Run(
      PublicKey(token, std::move(pkcs11_id),
                PublicKeySpki(SECItemToBytes(std::move(public_key_der)))));
}

void ImportCertOnWorkerThread(crypto::ScopedPK11Slot slot,
                              CertDer cert_der,
                              Kcer::Kcer::StatusCallback callback) {
  net::ScopedCERTCertificateList certs =
      net::x509_util::CreateCERTCertificateListFromBytes(
          reinterpret_cast<char*>(cert_der->data()), cert_der->size(),
          net::X509Certificate::FORMAT_AUTO);

  if (certs.empty() || (certs.size() != 1)) {
    return std::move(callback).Run(
        base::unexpected(Error::kInvalidCertificate));
  }

  if (int res = net::x509_util::ImportUserCert(certs[0].get());
      res != net::OK) {
    LOG(ERROR) << "Failed to import certificate, error: " << res;
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToImportCertificate));
  }

  return std::move(callback).Run({});
}

void ListCertsOnWorkerThread(
    crypto::ScopedPK11Slot slot,
    base::OnceCallback<void(net::ScopedCERTCertificateList)> callback) {
  crypto::ScopedCERTCertList cert_list(PK11_ListCertsInSlot(slot.get()));

  net::ScopedCERTCertificateList result;

  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
    result.push_back(net::x509_util::DupCERTCertificate(node->cert));
  }

  std::move(callback).Run(std::move(result));
}

void RemoveCertOnWorkerThread(crypto::ScopedPK11Slot slot,
                              scoped_refptr<const Cert> cert,
                              Kcer::StatusCallback callback) {
  net::ScopedCERTCertificate nss_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(
          cert->GetX509Cert().get());
  if (!nss_cert) {
    return std::move(callback).Run(
        base::unexpected(Error::kInvalidCertificate));
  }

  if (SEC_DeletePermCertificate(nss_cert.get()) != SECSuccess) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToRemoveCertificate));
  }
  // TODO(miersh): Currently the method returns "success" even when
  // SEC_DeletePermCertificate doesn't find the certificate. This is acceptable
  // for a "remove" method, but it might be useful to change it after NSS is not
  // used for Kcer.
  std::move(callback).Run({});
}

scoped_refptr<const Cert> BuildKcerCert(
    Token token,
    const net::ScopedCERTCertificate& nss_cert) {
  Pkcs11Id id_bytes(SECItemToBytes(crypto::MakeNssIdFromSpki(base::make_span(
      nss_cert->derPublicKey.data, nss_cert->derPublicKey.len))));

  return base::MakeRefCounted<Cert>(
      token, std::move(id_bytes), nss_cert->nickname,
      net::x509_util::CreateX509CertificateFromCERTCertificate(nss_cert.get()));
}

}  // namespace

KcerTokenImplNss::KcerTokenImplNss(Token token) : token_(token) {}

KcerTokenImplNss::~KcerTokenImplNss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::CertDatabase::GetInstance()->RemoveObserver(this);
}

void KcerTokenImplNss::Initialize(crypto::ScopedPK11Slot nss_slot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (nss_slot) {
    slot_ = std::move(nss_slot);
    net::CertDatabase::GetInstance()->AddObserver(this);
  } else {
    state_ = State::kInitializationFailed;
  }

  // This is supposed to be the first time the task queue is unblocked, no other
  // tasks should be already running.
  UnblockQueueProcessNextTask();
}

base::WeakPtr<KcerTokenImplNss> KcerTokenImplNss::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void KcerTokenImplNss::OnCertDBChanged() {
  state_ = State::kCacheOutdated;

  // If task queue is not blocked, trigger cache update immediately.
  if (!is_blocked_) {
    UnblockQueueProcessNextTask();
  }
}

void KcerTokenImplNss::GenerateRsaKey(uint32_t modulus_length_bits,
                                      bool hardware_backed,
                                      Kcer::GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (UNLIKELY(state_ == State::kInitializationFailed)) {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push(base::BindOnce(
        &KcerTokenImplNss::GenerateRsaKey, weak_factory_.GetWeakPtr(),
        modulus_length_bits, hardware_backed, std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GenerateRsaKeyOnWorkerThread, token_,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     modulus_length_bits, hardware_backed,
                     std::move(unblocking_callback)));
}

void KcerTokenImplNss::GenerateEcKey(EllipticCurve curve,
                                     bool hardware_backed,
                                     Kcer::GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (UNLIKELY(state_ == State::kInitializationFailed)) {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push(base::BindOnce(
        &KcerTokenImplNss::GenerateEcKey, weak_factory_.GetWeakPtr(), curve,
        hardware_backed, std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GenerateEcKeyOnWorkerThread, token_,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     curve, hardware_backed, std::move(unblocking_callback)));
}

void KcerTokenImplNss::ImportKey(
    Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
    Kcer::ImportKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (UNLIKELY(state_ == State::kInitializationFailed)) {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push(base::BindOnce(
        &KcerTokenImplNss::ImportKey, weak_factory_.GetWeakPtr(),
        std::move(pkcs8_private_key_info_der), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ImportKeyOnWorkerThread, token_,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(pkcs8_private_key_info_der),
                     std::move(unblocking_callback)));
}

void KcerTokenImplNss::ImportCertFromBytes(CertDer cert_der,
                                           Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (UNLIKELY(state_ == State::kInitializationFailed)) {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push(base::BindOnce(
        &KcerTokenImplNss::ImportCertFromBytes, weak_factory_.GetWeakPtr(),
        std::move(cert_der), std::move(callback)));
  }

  // Block task queue, attach queue unblocking and notification sending to the
  // callback.
  auto wrapped_callback = base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(&KcerTokenImplNss::OnCertsModified,
                     weak_factory_.GetWeakPtr(),
                     std::move(callback).Then(BlockQueueGetUnblocker())));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ImportCertOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(cert_der), std::move(wrapped_callback)));
}

void KcerTokenImplNss::ImportPkcs12Cert(Pkcs12Blob pkcs12_blob,
                                        std::string password,
                                        bool hardware_backed,
                                        Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                                        Kcer::ExportPkcs12Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::RemoveKeyAndCerts(PrivateKeyHandle key,
                                         Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::RemoveCert(scoped_refptr<const Cert> cert,
                                  Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (UNLIKELY(state_ == State::kInitializationFailed)) {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push(base::BindOnce(
        &KcerTokenImplNss::RemoveCert, weak_factory_.GetWeakPtr(),
        std::move(cert), std::move(callback)));
  }

  // Block task queue, attach queue unblocking and notification sending to the
  // callback.
  auto wrapped_callback = base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(&KcerTokenImplNss::OnCertsModified,
                     weak_factory_.GetWeakPtr(),
                     std::move(callback).Then(BlockQueueGetUnblocker())));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RemoveCertOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(cert), std::move(wrapped_callback)));
}

void KcerTokenImplNss::ListKeys(TokenListKeysCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::ListCerts(TokenListCertsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (UNLIKELY(state_ == State::kInitializationFailed)) {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push(base::BindOnce(&KcerTokenImplNss::ListCerts,
                                           weak_factory_.GetWeakPtr(),
                                           std::move(callback)));
  }
  return std::move(callback)
      .Then(BlockQueueGetUnblocker())
      .Run(cert_cache_.GetAllCerts());
}

void KcerTokenImplNss::DoesPrivateKeyExist(
    PrivateKeyHandle key,
    Kcer::DoesKeyExistCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::Sign(PrivateKeyHandle key,
                            SigningScheme signing_scheme,
                            DataToSign data,
                            Kcer::SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::SignRsaPkcs1Raw(PrivateKeyHandle key,
                                       SigningScheme signing_scheme,
                                       DigestWithPrefix digest_with_prefix,
                                       Kcer::SignCallback callback) {
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::GetTokenInfo(Kcer::GetTokenInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::GetKeyInfo(PrivateKeyHandle key,
                                  Kcer::GetKeyInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::SetKeyNickname(PrivateKeyHandle key,
                                      std::string nickname,
                                      Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::SetKeyPermissions(PrivateKeyHandle key,
                                         chaps::KeyPermissions key_permissions,
                                         Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::SetCertProvisioningProfileId(
    PrivateKeyHandle key,
    std::string profile_id,
    Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::OnCertsModified(Kcer::StatusCallback callback,
                                       base::expected<void, Error> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (result.has_value()) {
    net::CertDatabase::GetInstance()->NotifyObserversCertDBChanged();
  }
  // The Notify... above will post a task to invalidate the cache. Calling the
  // original callback for a request will automatically trigger updating cache
  // and executing the next request. Post a task with the original callback
  // (instead of calling it synchronously), so the cache update and the next
  // request happen after the notification.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

base::OnceClosure KcerTokenImplNss::BlockQueueGetUnblocker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  CHECK(!is_blocked_);
  is_blocked_ = true;

  // `unblocker` is executed either manually or on destruction.
  base::ScopedClosureRunner unblocker(
      base::BindOnce(&KcerTokenImplNss::UnblockQueueProcessNextTask,
                     weak_factory_.GetWeakPtr()));
  // Pack `unblocker` into an IO thread bound closure, so it can be attached to
  // a callback.
  return base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(&RunUnblocker, std::move(unblocker)));
}

void KcerTokenImplNss::UnblockQueueProcessNextTask() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  is_blocked_ = false;

  if (state_ == State::kCacheOutdated) {
    return UpdateCache();
  }

  if (task_queue_.empty()) {
    return;
  }

  base::OnceClosure next_task = std::move(task_queue_.front());
  task_queue_.pop();
  std::move(next_task).Run();
}

void KcerTokenImplNss::UpdateCache() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Block task queue, create callback for the current sequence.
  auto callback =
      base::BindPostTask(content::GetIOThreadTaskRunner({}),
                         base::BindOnce(&KcerTokenImplNss::UpdateCacheWithCerts,
                                        weak_factory_.GetWeakPtr())
                             .Then(BlockQueueGetUnblocker()));

  state_ = State::kCacheUpdating;

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ListCertsOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(callback)));
}

void KcerTokenImplNss::UpdateCacheWithCerts(
    net::ScopedCERTCertificateList new_certs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kCacheOutdated) {
    // If the status switched from kUpdating, then new update happened since the
    // cache started to update, `new_certs` might already be outdated. Skip
    // re-building the cache and try again.
    return;
  }

  std::vector<scoped_refptr<const Cert>> new_cache;

  // For every cert that was found, either take it from the previous cache or
  // create a kcer::Cert object for it.
  for (const net::ScopedCERTCertificate& new_cert : new_certs) {
    scoped_refptr<const Cert> cert = cert_cache_.FindCert(new_cert);
    if (!cert) {
      cert = BuildKcerCert(token_, new_cert);
    }
    new_cache.push_back(std::move(cert));
  }

  // Rebuilding the cache implicitly removes all the certs that are not in the
  // permanent storage anymore. The certs themself will be fully destroyed when
  // the last ref-counting reference to them is destroyed.
  cert_cache_ = CertCacheNss(new_cache);
  state_ = State::kCacheUpToDate;
}

template <typename T>
void KcerTokenImplNss::HandleInitializationFailed(
    base::OnceCallback<void(base::expected<T, Error>)> callback) {
  std::move(callback).Run(base::unexpected(Error::kTokenInitializationFailed));
  // Multiple tasks might be handled in a row, schedule the next task
  // asynchronously to not overload the stack and not occupy the thread for too
  // long.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&KcerTokenImplNss::UnblockQueueProcessNextTask,
                                weak_factory_.GetWeakPtr()));
}

}  // namespace kcer::internal
