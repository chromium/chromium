// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CERT_DATABASE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CERT_DATABASE_ASH_H_

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/login/login_state/login_state.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
class TPMTokenInfoGetter;
}  // namespace chromeos

namespace crosapi {

// Implements the crosapi interface for certificate database. Lives in
// Ash-Chrome on the UI thread.
//
// It is expected that during Lacros-Chrome initialization when it creates the
// main profile (that contains device account), it will call GetCertDatabaseInfo
// mojo API. If the ChromeOS user session was just started, it can take time for
// Ash-Chrome to initialize TPM and certificate database. When it is done, the
// API call will be resolved. If Lacros-Chrome is restarted, it will call
// GetCertDatabaseInfo again and receive a cached result from the first call.
// The cached result is reset on login state change (i.e. sign in / sign out).
class CertDatabaseAsh : public mojom::CertDatabase,
                        chromeos::LoginState::Observer {
 public:
  CertDatabaseAsh();
  CertDatabaseAsh(const CertDatabaseAsh&) = delete;
  CertDatabaseAsh& operator=(const CertDatabaseAsh&) = delete;
  ~CertDatabaseAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::CertDatabase> receiver);

  // Returns to Lacros-Chrome all necessary data to initialize certificate
  // database when it is ready. Caches the result of first call for all
  // subsequent calls during current user session.
  void GetCertDatabaseInfo(GetCertDatabaseInfoCallback callback) override;

 private:
  // chromeos::LoginState::Observer
  void LoggedInStateChanged() override;

  // The fact that TpmTokenInfo can be retrieved is used as a signal that
  // certificate database is ready to be initialized in Lacros-Chrome.
  void WaitForTpmTokenReady(GetCertDatabaseInfoCallback callback);
  void OnTpmTokenReady(
      std::unique_ptr<chromeos::TPMTokenInfoGetter> token_getter,
      GetCertDatabaseInfoCallback callback,
      base::Optional<user_data_auth::TpmTokenInfo> token_info);

  base::Optional<bool> is_tpm_token_ready_;

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::CertDatabase> receivers_;

  base::WeakPtrFactory<CertDatabaseAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CERT_DATABASE_ASH_H_
