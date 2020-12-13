// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "crypto/scoped_nss_types.h"

namespace net {
class NSSCertDatabase;
}

namespace chromeos {

// An observer that gets notified when the global NSSCertDatabase is about to be
// destroyed.
class SystemTokenCertDBObserver : public base::CheckedObserver {
 public:
  // Called when the global NSSCertDatabase is about to be destroyed.
  // Consumers of that database should drop any reference to it and stop using
  // it.
  virtual void OnSystemTokenCertDBDestroyed() = 0;
};

// Initializes a global NSSCertDatabase for the system token and starts
// NetworkCertLoader with that database.
//
// Lifetime: The global NetworkCertLoader instance must exist until ShutDown()
// has been called. The global NetworkCertLoader instance must exist until
// ShutDown() has been called, but must be outlived by this object.
//
// All of the methods must be called on the UI thread.
class SystemTokenCertDBInitializer : public TpmManagerClient::Observer {
 public:
  // It is stated in cryptohome implementation that 5 minutes is enough time to
  // wait for any TPM operations. For more information, please refer to:
  // https://chromium.googlesource.com/chromiumos/platform2/+/master/cryptohome/cryptohome.cc
  static constexpr base::TimeDelta kMaxCertDbRetrievalDelay =
      base::TimeDelta::FromMinutes(5);

  SystemTokenCertDBInitializer();
  ~SystemTokenCertDBInitializer() override;

  // Returns a global instance. May return null if not initialized.
  static SystemTokenCertDBInitializer* Get();

  // Stops making new requests to D-Bus services.
  void ShutDown();

  // TpmManagerClient::Observer overrides.
  void OnOwnershipTaken() override;

  // Retrieves the global NSSCertDatabase for the system token and passes it to
  // |callback|. If the database is already initialized, calls |callback|
  // immediately. Otherwise, |callback| will be called when the database is
  // initialized.
  // To be notified when the returned NSSCertDatabase becomes invalid, callers
  // should register as SystemTokenCertDBObserver.

  using GetSystemTokenCertDbCallback =
      base::OnceCallback<void(net::NSSCertDatabase* nss_cert_database)>;
  void GetSystemTokenCertDb(GetSystemTokenCertDbCallback callback);

  // Adds |observer| as SystemTokenCertDBObserver.
  void AddObserver(SystemTokenCertDBObserver* observer);
  // Removes |observer| as SystemTokenCertDBObserver.
  void RemoveObserver(SystemTokenCertDBObserver* observer);

  // Sets if the software fallback for system slot is allowed; useful for
  // testing.
  void set_is_system_slot_software_fallback_allowed(bool is_allowed) {
    is_system_slot_software_fallback_allowed_ = is_allowed;
  }

 private:
  // Called once the cryptohome service is available.
  void OnCryptohomeAvailable(bool available);

  // Verifies the value of the build flag system_slot_software_fallback and
  // decides the initialization flow based on that.
  void CheckTpm();

  // If GetTpmNonsensitiveStatus() fails (e.g. if TPM token is not yet ready)
  // schedules the initialization step retry attempt after a timeout.
  void RetryCheckTpmLater();

  // This is a callback for the GetTpmNonsensitiveStatus() query. 2 main
  // operations are performed:
  // 1. Initializes the database if TPM is owned or software fallback is
  // enabled.
  // 2. Triggers TPM ownership process if necessary.
  void OnGetTpmNonsensitiveStatus(
      const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply);

  // Starts loading the system slot and initializing the corresponding NSS cert
  // database, unless it was already started before.
  void MaybeStartInitializingDatabase();

  // Initializes the global system token NSSCertDatabase with |system_slot|.
  // Also starts NetworkCertLoader with the system token database.
  void InitializeDatabase(crypto::ScopedPK11Slot system_slot);

  // Called after a delay if the system token certificate database was still not
  // initialized when |GetSystemTokenCertDb| was called. This function notifies
  // |get_system_token_cert_db_callback_list_| with nullptrs as a way of
  // informing callers that the database initialization failed.
  void OnSystemTokenDbRetrievalTimeout();

  // Whether the database initialization was started.
  bool started_initializing_ = false;

  // Global NSSCertDatabase which sees the system token.
  std::unique_ptr<net::NSSCertDatabase> system_token_cert_database_;

  // List of callbacks that should be executed when the system token certificate
  // database is created.
  base::OnceCallbackList<GetSystemTokenCertDbCallback::RunType>
      get_system_token_cert_db_callback_list_;

  // List of observers that will be notified when the global system token
  // NSSCertDatabase is destroyed.
  base::ObserverList<SystemTokenCertDBObserver> observers_;

  bool system_token_cert_db_retrieval_failed_ = false;

  base::OneShotTimer system_token_cert_db_retrieval_timer_;

  // The current request delay before the next attempt to retrieve the TPM
  // state. Will be adapted after each attempt.
  base::TimeDelta tpm_request_delay_;

  // The flag that determines if the system slot can use software fallback.
  bool is_system_slot_software_fallback_allowed_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SystemTokenCertDBInitializer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemTokenCertDBInitializer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_
