// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CERT_DATABASE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CERT_DATABASE_ASH_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/components/certificate_provider/certificate_info.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

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
class CertDatabaseAsh : public mojom::CertDatabase, ash::LoginState::Observer {
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

  // mojom::CertDatabase
  void OnCertsChangedInLacros(
      mojom::CertDatabaseChangeType change_type) override;
  void AddAshCertDatabaseObserver(
      mojo::PendingRemote<mojom::AshCertDatabaseObserver> observer) override;
  void SetCertsProvidedByExtension(
      const std::string& extension_id,
      const chromeos::certificate_provider::CertificateInfoList&
          certificate_infos) override;
  void OnPkcs12CertDualWritten() override;

  // Notifies observers that were added with `AddAshCertDatabaseObserver` about
  // cert changes in Ash.
  void NotifyCertsChangedInAsh(mojom::CertDatabaseChangeType change_type);

 private:
  // ash::LoginState::Observer
  void LoggedInStateChanged() override;

  void WaitForCertDatabaseReady(GetCertDatabaseInfoCallback callback);
  void OnCertDatabaseReady(GetCertDatabaseInfoCallback callback,
                           unsigned long private_slot_id,
                           std::optional<unsigned long> system_slot_id);

  std::optional<bool> is_cert_database_ready_;
  unsigned long private_slot_id_;
  std::optional<unsigned long> system_slot_id_;

  // The observers that will receive notifications about cert changes in Ash.
  mojo::RemoteSet<mojom::AshCertDatabaseObserver> observers_;

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::CertDatabase> receivers_;

  base::WeakPtrFactory<CertDatabaseAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CERT_DATABASE_ASH_H_
