// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_H_
#define CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/net/server_certificate_database.pb.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

// Wraps the SQLite database that provides on-disk storage for user-configured
// TLS certificates. This class is expected to be created and accessed on a
// backend sequence.
class ServerCertificateDatabase {
 public:
  struct CertInformation {
    CertInformation();
    ~CertInformation();
    CertInformation(CertInformation&&);
    CertInformation& operator=(CertInformation&& other);

    std::string sha256hash_hex;
    std::vector<uint8_t> der_cert;
    chrome_browser_server_certificate_database::CertificateMetadata
        cert_metadata;
  };

  // Opens the database in `storage_dir`, creating it if one does not exist.
  // `storage_dir` will generally be in the Profile directory.
  explicit ServerCertificateDatabase(const base::FilePath& storage_dir);

  ServerCertificateDatabase(const ServerCertificateDatabase&) = delete;
  ServerCertificateDatabase& operator=(const ServerCertificateDatabase&) =
      delete;
  ~ServerCertificateDatabase();

  static std::optional<bssl::CertificateTrustType> GetUserCertificateTrust(
      const net::ServerCertificateDatabase::CertInformation& cert_info);

  // Insert a new certificate into the database, or if the certificate is
  // already present (as indicated by cert_info.sha256hash_hex), update the
  // entry in the database.
  bool InsertOrUpdateCert(const CertInformation& cert_info);

  // Retrieve all of the certificates from the database.
  std::vector<CertInformation> RetrieveAllCertificates();

 private:
  sql::InitStatus InitInternal(const base::FilePath& storage_dir);

  // The underlying SQL database.
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_H_
