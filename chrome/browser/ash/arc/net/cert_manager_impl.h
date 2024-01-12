// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NET_CERT_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_NET_CERT_MANAGER_IMPL_H_

#include <optional>
#include <string>

#include "ash/components/arc/net/cert_manager.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/profiles/profile.h"
#include "net/cert/nss_cert_database.h"

namespace arc {

// Certificate and private key PKCS #8 PEM headers as described in section 5 and
// 10 respectively of RFC7468.
constexpr char kCertificatePEMHeader[] = "CERTIFICATE";
constexpr char kPrivateKeyPEMHeader[] = "PRIVATE KEY";

// CertManager imports plain-text certificates and private keys into Chrome OS'
// key store (chaps).
class CertManagerImpl : public CertManager {
 public:
  explicit CertManagerImpl(Profile* profile);

  CertManagerImpl(const CertManagerImpl&) = delete;
  CertManagerImpl& operator=(const CertManagerImpl&) = delete;

  ~CertManagerImpl() override;

  // Asynchronously import a PEM-formatted private key and user certificate into
  // the NSS certificate database. Once done, |callback| will be called with its
  // ID and the slot ID of the database. This method will asynchronously fetch
  // the database. Calling this method will remove any previously imported
  // private keys and certificates with the same ID.
  // For Passpoint, the expected removal flow of private keys and certificates
  // are done in shill directly using PKCS#11 API. This means that any state of
  // NSS for the private keys and certificates are not cleaned. This resulted in
  // any subsequent provisionings of a deleted certificate to fail. In order to
  // not have the side effect, the removal is necessary.
  void ImportPrivateKeyAndCert(
      const std::string& key_pem,
      const std::string& cert_pem,
      ImportPrivateKeyAndCertCallback callback) override;

 private:
  // Imports a PEM-formatted private key into the NSS certificate database and
  // return its ID or empty string if it fails.
  std::string ImportPrivateKey(const std::string& key_pem,
                               net::NSSCertDatabase* database);

  // Imports a PEM-formatted user certificate into the NSS certificate database
  // and return its ID or empty string if it fails.
  std::string ImportUserCert(const std::string& cert_pem,
                             net::NSSCertDatabase* database);

  void DeleteCertAndKey(const std::string& cert_pem,
                        net::NSSCertDatabase* database);

  // Get the private slot ID used by this class.
  int GetSlotID(net::NSSCertDatabase* database);

  // Import a PEM-formatted private key and user certificate into the NSS
  // certificate database. Calls a callback with its ID and the slot ID of the
  // database.
  void ImportPrivateKeyAndCertWithDB(const std::string& key_pem,
                                     const std::string& cert_pem,
                                     ImportPrivateKeyAndCertCallback callback,
                                     net::NSSCertDatabase* database);
  raw_ptr<Profile, DanglingUntriaged> profile_;
  base::WeakPtrFactory<CertManagerImpl> weak_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(CertManagerImplTest, ImportKeyAndCertTest);
  FRIEND_TEST_ALL_PREFIXES(CertManagerImplTest, ImportKeyAndCertSameIDTest);
  FRIEND_TEST_ALL_PREFIXES(CertManagerImplTest, ImportCertWithWrongKeyTest);
  FRIEND_TEST_ALL_PREFIXES(CertManagerImplTest,
                           ImportKeyAndCertWithInvalidDBTest);
  FRIEND_TEST_ALL_PREFIXES(CertManagerImplTest, ImportInvalidDataTest);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NET_CERT_MANAGER_IMPL_H_
