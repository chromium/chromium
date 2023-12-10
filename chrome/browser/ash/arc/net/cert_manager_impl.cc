// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/net/cert_manager_impl.h"

#include <pk11priv.h>
#include <pk11pub.h>

#include <optional>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_key_util.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace {

void GetCertDBOnIOThread(
    NssCertDatabaseGetter database_getter,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(std::move(split_callback.first));
  // If the NSS database was already available, |cert_db| is non-null and
  // the callback has not been called. Explicitly call the callback.
  if (cert_db) {
    std::move(split_callback.second).Run(cert_db);
  }
}

net::ScopedCERTCertificate TranslatePEMToCert(const std::string& cert_pem) {
  bssl::PEMTokenizer tokenizer(cert_pem, {arc::kCertificatePEMHeader});
  if (!tokenizer.GetNext()) {
    NET_LOG(ERROR) << "Failed to get certificate data";
    return nullptr;
  }

  std::vector<uint8_t> cert_der(tokenizer.data().begin(),
                                tokenizer.data().end());
  return net::x509_util::CreateCERTCertificateFromBytes(cert_der);
}

}  // namespace

namespace arc {

CertManagerImpl::CertManagerImpl(Profile* profile) : profile_(profile) {}

CertManagerImpl::~CertManagerImpl() = default;

std::string CertManagerImpl::ImportPrivateKey(const std::string& key_pem,
                                              net::NSSCertDatabase* database) {
  if (!database) {
    NET_LOG(ERROR) << "Certificate database is not initialized";
    return std::string();
  }

  bssl::PEMTokenizer tokenizer(key_pem, {kPrivateKeyPEMHeader});
  if (!tokenizer.GetNext()) {
    NET_LOG(ERROR) << "Failed to get private key data";
    return std::string();
  }
  std::vector<uint8_t> key_der(tokenizer.data().begin(),
                               tokenizer.data().end());

  crypto::ScopedPK11Slot private_slot = database->GetPrivateSlot();
  if (!private_slot) {
    NET_LOG(ERROR) << "Failed to get PK11 slot";
    return std::string();
  }

  crypto::ScopedSECKEYPrivateKey key(crypto::ImportNSSKeyFromPrivateKeyInfo(
      private_slot.get(), key_der, true /* permanent */));
  if (!key) {
    NET_LOG(ERROR) << "Failed to import private key";
    return std::string();
  }

  crypto::ScopedSECItem sec_item(PK11_GetLowLevelKeyIDForPrivateKey(key.get()));
  if (!sec_item) {
    NET_LOG(ERROR) << "Failed to get private key ID";
    return std::string();
  }
  return base::HexEncode(sec_item->data, sec_item->len);
}

std::string CertManagerImpl::ImportUserCert(const std::string& cert_pem,
                                            net::NSSCertDatabase* database) {
  if (!database) {
    NET_LOG(ERROR) << "Certificate database is not initialized";
    return std::string();
  }

  net::ScopedCERTCertificate cert = TranslatePEMToCert(cert_pem);
  if (!cert) {
    NET_LOG(ERROR) << "Failed to translate PEM to certificate object";
    return std::string();
  }

  int status = database->ImportUserCert(cert.get());
  if (status != net::OK) {
    NET_LOG(ERROR) << "Failed to import user certificate with status code "
                   << status;
    return std::string();
  }

  crypto::ScopedSECItem sec_item(
      PK11_GetLowLevelKeyIDForCert(nullptr, cert.get(), nullptr));
  if (!sec_item) {
    NET_LOG(ERROR) << "Failed to get certificate ID";
    return std::string();
  }

  return base::HexEncode(sec_item->data, sec_item->len);
}

void CertManagerImpl::DeleteCertAndKey(const std::string& cert_pem,
                                       net::NSSCertDatabase* database) {
  if (!database) {
    NET_LOG(ERROR) << "Certificate database is not initialized";
    return;
  }

  net::ScopedCERTCertificate cert = TranslatePEMToCert(cert_pem);
  if (!cert) {
    NET_LOG(ERROR) << "Failed to translate PEM to certificate object";
    return;
  }
  database->DeleteCertAndKey(cert.get());
}

int CertManagerImpl::GetSlotID(net::NSSCertDatabase* database) {
  if (!database) {
    NET_LOG(ERROR) << "Certificate database is not initialized";
    return -1;
  }

  crypto::ScopedPK11Slot private_slot = database->GetPrivateSlot();
  if (!private_slot) {
    NET_LOG(ERROR) << "Failed to get PK11 slot";
    return -1;
  }

  return PK11_GetSlotID(private_slot.get());
}

void CertManagerImpl::ImportPrivateKeyAndCertWithDB(
    const std::string& key_pem,
    const std::string& cert_pem,
    ImportPrivateKeyAndCertCallback callback,
    net::NSSCertDatabase* database) {
  // Attempt to delete the key and certificate first. This is important for an
  // edge case below.
  // 1. The user removes a set of passpoint credentials. This causes shill to
  // delete the key and certificate from chaps.
  // 2. The user re-adds passpoint credentials with the same key and
  // certificate.
  // If there is no Chrome restart between (1) and (2), NSS caches are not
  // updated with the result of (1), making (2) fail.
  // Deleting the key from NSS ensures that (2) succeeds even in this case.
  DeleteCertAndKey(cert_pem, database);
  std::string key_id = ImportPrivateKey(key_pem, database);
  if (key_id.empty()) {
    NET_LOG(ERROR) << "Failed to import private key";
    std::move(callback).Run(/*cert_id=*/std::nullopt,
                            /*slot_id=*/std::nullopt);
    return;
  }
  // Both DeleteCertAndKey parse the passed certificate into a CERTCertificate.
  // This is unfortunate but reusing the same CERTCertificate resulted in
  // PK11_GetLowLevelKeyIDForCert failing after the DeleteCertAndKey call.
  std::string cert_id = ImportUserCert(cert_pem, database);
  if (cert_id.empty()) {
    NET_LOG(ERROR) << "Failed to import client certificate";
    std::move(callback).Run(/*cert_id=*/std::nullopt,
                            /*slot_id=*/std::nullopt);
    return;
  }
  int slot_id = GetSlotID(database);

  // The ID of imported user certificate and private key is the same, use one
  // of them.
  DCHECK(key_id == cert_id);
  std::move(callback).Run(cert_id, slot_id);
}

void CertManagerImpl::ImportPrivateKeyAndCert(
    const std::string& key_pem,
    const std::string& cert_pem,
    ImportPrivateKeyAndCertCallback callback) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetCertDBOnIOThread,
                     NssServiceFactory::GetForContext(profile_)
                         ->CreateNSSCertDatabaseGetterForIOThread(),
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &CertManagerImpl::ImportPrivateKeyAndCertWithDB,
                         weak_factory_.GetWeakPtr(), key_pem, cert_pem,
                         std::move(callback)))));
}

}  // namespace arc
