// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_SERVICE_H_
#define CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_SERVICE_H_

#include "base/functional/callback.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/net/server_certificate_database.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace net {

// KeyedService that loads and provides policies around usage of Certificates
// for TLS.
class ServerCertificateDatabaseService : public KeyedService {
 public:
  explicit ServerCertificateDatabaseService(Profile* profile);

  ServerCertificateDatabaseService(const ServerCertificateDatabaseService&) =
      delete;
  ServerCertificateDatabaseService& operator=(
      const ServerCertificateDatabaseService&) = delete;

  ~ServerCertificateDatabaseService() override;

  // Add or update user settings with the included certificate.
  void AddOrUpdateUserCertificate(
      net::ServerCertificateDatabase::CertInformation cert_info,
      base::OnceCallback<void(bool)> callback);

  // Read all certificates from the database.
  void GetAllCertificates(
      base::OnceCallback<
          void(std::vector<net::ServerCertificateDatabase::CertInformation>)>
          callback);

  // Run callback with `server_cert_database_`. The callback will be run on a
  // thread pool sequence where it is allowed to call methods on the database
  // object. This can be used to do multiple operations on the database without
  // repeated thread hops.
  void PostTaskWithDatabase(
      base::OnceCallback<void(net::ServerCertificateDatabase*)> callback);

 private:
  const raw_ptr<Profile> profile_;

  base::SequenceBound<net::ServerCertificateDatabase> server_cert_database_;
};

}  // namespace net

#endif  // CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_SERVICE_H_
