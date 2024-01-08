// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_PLATFORM_KEYS_HELPERS_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_PLATFORM_KEYS_HELPERS_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "net/cert/x509_certificate.h"

namespace ash {

namespace platform_keys {
class PlatformKeysService;
}

namespace cert_provisioning {

// ========= CertIterator ======================================================

using CertIteratorForEachCallback =
    base::RepeatingCallback<void(scoped_refptr<net::X509Certificate> cert,
                                 const CertProfileId& cert_profile_id,
                                 chromeos::platform_keys::Status status)>;
using CertIteratorOnFinishedCallback =
    base::OnceCallback<void(chromeos::platform_keys::Status status)>;

// Iterates over all existing certificates of a given |cert_scope| and combines
// them with their certificate provisioning ids when possible. Runs |callback|
// on every (cert, cert_profile_id) pair that had a present and non-empty
// |cert_profile_id|. If |error_message| is not empty, then the pair is not
// valid.
class CertIterator {
 public:
  CertIterator(CertScope cert_scope,
               platform_keys::PlatformKeysService* platform_keys_service);
  CertIterator(const CertIterator&) = delete;
  CertIterator& operator=(const CertIterator&) = delete;
  ~CertIterator();

  // Can be called more than once. If previous iteration is not finished, it
  // will be canceled.
  void IterateAll(CertIteratorForEachCallback for_each_callback,
                  CertIteratorOnFinishedCallback on_finished_callback);
  void Cancel();

 private:
  void OnGetCertificatesDone(
      std::unique_ptr<net::CertificateList> existing_certs,
      chromeos::platform_keys::Status status);
  void OnGetAttributeForKeyDone(scoped_refptr<net::X509Certificate> cert,
                                std::optional<std::vector<uint8_t>> attr_value,
                                chromeos::platform_keys::Status status);
  void StopIteration(chromeos::platform_keys::Status status);

  const CertScope cert_scope_ = CertScope::kDevice;
  const raw_ptr<platform_keys::PlatformKeysService> platform_keys_service_ =
      nullptr;

  size_t wait_counter_ = 0;
  CertIteratorForEachCallback for_each_callback_;
  CertIteratorOnFinishedCallback on_finished_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CertIterator> weak_factory_{this};
};

// ========= LatestCertsWithIdsGetter ==========================================

using LatestCertsWithIdsGetterCallback = base::OnceCallback<void(
    base::flat_map<CertProfileId, scoped_refptr<net::X509Certificate>>
        certs_with_ids,
    chromeos::platform_keys::Status status)>;

// Collects map of certificates with their certificate provisioning ids and
// returns it via |callback|. If there are several certificates for the same id,
// only the newest one will be stored in the map. Only one call to
// GetCertsWithIds() for one instance is allowed.
class LatestCertsWithIdsGetter {
 public:
  LatestCertsWithIdsGetter(
      CertScope cert_scope,
      platform_keys::PlatformKeysService* platform_keys_service);
  LatestCertsWithIdsGetter(const LatestCertsWithIdsGetter&) = delete;
  LatestCertsWithIdsGetter& operator=(const LatestCertsWithIdsGetter&) = delete;
  ~LatestCertsWithIdsGetter();

  // Can be called more than once. If previous task is not finished, it will be
  // canceled.
  void GetCertsWithIds(LatestCertsWithIdsGetterCallback callback);
  bool IsRunning() const;
  void Cancel();

 private:
  void ProcessOneCert(scoped_refptr<net::X509Certificate> new_cert,
                      const CertProfileId& cert_profile_id,
                      chromeos::platform_keys::Status status);
  void OnIterationFinished(chromeos::platform_keys::Status status);

  CertIterator iterator_;

  // Accumulates results that will be returned at the end via |callback_|.
  base::flat_map<CertProfileId, scoped_refptr<net::X509Certificate>>
      certs_with_ids_;
  LatestCertsWithIdsGetterCallback callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LatestCertsWithIdsGetter> weak_factory_{this};
};

// ========= CertDeleter =======================================================

using CertDeleterCallback =
    base::OnceCallback<void(chromeos::platform_keys::Status status)>;

// Finds and deletes certificates that 1) have ids that are not in
// |cert_profile_ids_to_keep| set or 2) have another certificate for the same
// id with later expiration date. Only one call to DeleteCerts() for one
// instance is allowed.
class CertDeleter {
 public:
  CertDeleter(CertScope cert_scope,
              platform_keys::PlatformKeysService* platform_keys_service);
  CertDeleter(const CertDeleter&) = delete;
  CertDeleter& operator=(const CertDeleter&) = delete;
  ~CertDeleter();
  void Cancel();

  // Can be called more than once. If previous task is not finished, it will be
  // canceled.
  void DeleteCerts(base::flat_set<CertProfileId> cert_profile_ids_to_keep,
                   CertDeleterCallback callback);

 private:
  void ProcessOneCert(scoped_refptr<net::X509Certificate> cert,
                      const CertProfileId& cert_profile_id,
                      chromeos::platform_keys::Status status);
  void RememberOrDelete(scoped_refptr<net::X509Certificate> new_cert,
                        const CertProfileId& cert_profile_id);
  void DeleteCert(scoped_refptr<net::X509Certificate> cert);
  void OnDeleteCertDone(chromeos::platform_keys::Status status);
  void OnIterationFinished(chromeos::platform_keys::Status status);
  void CheckStateAndMaybeFinish();
  void ReturnStatus(chromeos::platform_keys::Status status);

  const CertScope cert_scope_ = CertScope::kDevice;
  const raw_ptr<platform_keys::PlatformKeysService> platform_keys_service_ =
      nullptr;

  CertIterator iterator_;
  bool iteration_finished_ = false;
  size_t pending_delete_tasks_counter_ = 0;
  CertDeleterCallback callback_;

  // Contains list of currently existing certificate profile ids. Certificates
  // with ids outside of this set can be deleted.
  base::flat_set<CertProfileId> cert_profile_ids_to_keep_;

  // Stores previously seen certificates that allows to find duplicates.
  base::flat_map<CertProfileId, scoped_refptr<net::X509Certificate>>
      certs_with_ids_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CertDeleter> weak_factory_{this};
};

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_PLATFORM_KEYS_HELPERS_H_
