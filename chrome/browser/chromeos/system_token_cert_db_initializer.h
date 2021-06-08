// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "crypto/scoped_nss_types.h"

namespace net {
class NSSCertDatabase;
}

namespace chromeos {

// Initializes a global NSSCertDatabase for the system token and stores it in a
// global SystemTokenCertDbStorage instance which can be used by all ChromeOS
// components, i.e. components under //chrome/browser/chromeos/ and //chromeos/
//
// All of the methods must be called on the UI thread.
class SystemTokenCertDBInitializer : public TpmManagerClient::Observer {
 public:
  // It is stated in cryptohome implementation that 5 minutes is enough time to
  // wait for any TPM operations. For more information, please refer to:
  // https://chromium.googlesource.com/chromiumos/platform2/+/master/cryptohome/cryptohome.cc
  static constexpr base::TimeDelta kMaxCertDbRetrievalDelay =
      base::TimeDelta::FromMinutes(5);

  // Note: This should only be used by ChromeBrowserMainPartsChromeos to
  // initialize the system token certificate database. Use
  // SystemTokenCertDbStorage to retrieve the database.
  SystemTokenCertDBInitializer();
  ~SystemTokenCertDBInitializer() override;

  // TpmManagerClient::Observer overrides.
  void OnOwnershipTaken() override;

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

  // Whether the database initialization was started.
  bool started_initializing_ = false;

  // The current request delay before the next attempt to retrieve the TPM
  // state. Will be adapted after each attempt.
  base::TimeDelta tpm_request_delay_;

  // The flag that determines if the system slot can use software fallback.
  bool is_system_slot_software_fallback_allowed_;

  // Global NSSCertDatabase which sees the system token.
  std::unique_ptr<net::NSSCertDatabase> system_token_cert_database_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SystemTokenCertDBInitializer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemTokenCertDBInitializer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TOKEN_CERT_DB_INITIALIZER_H_
