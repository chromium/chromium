// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_token_cert_db_initializer.h"

#include <pk11pub.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/tpm/tpm_token_loader.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util_internal.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/nss_cert_database_chromeos.h"

namespace chromeos {

namespace {

// Called on UI Thread when the system slot has been retrieved.
void GotSystemSlotOnUIThread(
    base::OnceCallback<void(crypto::ScopedPK11Slot)> callback_ui_thread,
    crypto::ScopedPK11Slot system_slot) {
  std::move(callback_ui_thread).Run(std::move(system_slot));
}

// Called on IO Thread when the system slot has been retrieved.
void GotSystemSlotOnIOThread(
    base::OnceCallback<void(crypto::ScopedPK11Slot)> callback_ui_thread,
    crypto::ScopedPK11Slot system_slot) {
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&GotSystemSlotOnUIThread, std::move(callback_ui_thread),
                     std::move(system_slot)));
}

// Called on IO Thread, initiates retrieval of system slot. |callback_ui_thread|
// will be executed on the UI thread when the system slot has been retrieved.
void GetSystemSlotOnIOThread(
    base::RepeatingCallback<void(crypto::ScopedPK11Slot)> callback_ui_thread) {
  auto callback =
      base::BindRepeating(&GotSystemSlotOnIOThread, callback_ui_thread);
  crypto::ScopedPK11Slot system_nss_slot =
      crypto::GetSystemNSSKeySlot(callback);
  if (system_nss_slot) {
    callback.Run(std::move(system_nss_slot));
  }
}

// Decides if on start we shall signal to the platform that it can attempt
// owning the TPM.
// For official Chrome builds, send this signal if EULA has been accepted
// already (i.e. the user has started OOBE) to make sure we are not stuck with
// uninitialized TPM after an interrupted OOBE process.
// For Chromium builds, don't send it here. Instead, rely on this signal being
// sent after each successful login.
bool ShallAttemptTpmOwnership() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return StartupUtils::IsEulaAccepted();
#else
  return false;
#endif
}

}  // namespace

SystemTokenCertDBInitializer::SystemTokenCertDBInitializer() {
  // Only start loading the system token once cryptohome is available and only
  // if the TPM is ready (available && owned && not being owned).
  CryptohomeClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&SystemTokenCertDBInitializer::OnCryptohomeAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

SystemTokenCertDBInitializer::~SystemTokenCertDBInitializer() = default;

void SystemTokenCertDBInitializer::ShutDown() {
  // Note that the observer could potentially not be added yet, but
  // RemoveObserver() is a no-op in that case.
  CryptohomeClient::Get()->RemoveObserver(this);
}

void SystemTokenCertDBInitializer::TpmInitStatusUpdated(
    bool ready,
    bool owned,
    bool was_owned_this_boot) {
  if (ready) {
    // The TPM "ready" means that it's available && owned && not being owned.
    MaybeStartInitializingDatabase();
  }
}

void SystemTokenCertDBInitializer::OnCryptohomeAvailable(bool available) {
  if (!available) {
    LOG(ERROR) << "SystemTokenCertDBInitializer: Failed to wait for "
                  "cryptohome to become available.";
    return;
  }

  VLOG(1) << "SystemTokenCertDBInitializer: Cryptohome available.";
  CryptohomeClient::Get()->AddObserver(this);
  CryptohomeClient::Get()->TpmIsReady(
      base::BindOnce(&SystemTokenCertDBInitializer::OnGotTpmIsReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemTokenCertDBInitializer::OnGotTpmIsReady(
    base::Optional<bool> tpm_is_ready) {
  if (!tpm_is_ready.has_value() || !tpm_is_ready.value()) {
    VLOG(1) << "SystemTokenCertDBInitializer: TPM is not ready - not loading "
               "system token.";
    if (ShallAttemptTpmOwnership()) {
      // Signal to cryptohome that it can attempt TPM ownership, if it
      // haven't done that yet. The previous signal from EULA dialogue could
      // have been lost if initialization was interrupted.
      // We don't care about the result, and don't block waiting for it.
      LOG(WARNING) << "Request attempting TPM ownership.";
      CryptohomeClient::Get()->TpmCanAttemptOwnership(
          EmptyVoidDBusMethodCallback());
    }

    return;
  }
  MaybeStartInitializingDatabase();
}

void SystemTokenCertDBInitializer::MaybeStartInitializingDatabase() {
  if (started_initializing_)
    return;
  started_initializing_ = true;
  VLOG(1)
      << "SystemTokenCertDBInitializer: TPM is ready, loading system token.";
  TPMTokenLoader::Get()->EnsureStarted();
  base::RepeatingCallback<void(crypto::ScopedPK11Slot)> callback =
      base::BindRepeating(&SystemTokenCertDBInitializer::InitializeDatabase,
                          weak_ptr_factory_.GetWeakPtr());
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(&GetSystemSlotOnIOThread, callback));
}

void SystemTokenCertDBInitializer::InitializeDatabase(
    crypto::ScopedPK11Slot system_slot) {
  // Currently, NSSCertDatabase requires a public slot to be set, so we use
  // the system slot there. We also want GetSystemSlot() to return the system
  // slot. As ScopedPK11Slot is actually a unique_ptr which will be moved into
  // the NSSCertDatabase, we need to create a copy, referencing the same slot
  // (using PK11_ReferenceSlot).
  crypto::ScopedPK11Slot system_slot_copy =
      crypto::ScopedPK11Slot(PK11_ReferenceSlot(system_slot.get()));
  auto database = std::make_unique<net::NSSCertDatabaseChromeOS>(
      /*public_slot=*/std::move(system_slot),
      /*private_slot=*/crypto::ScopedPK11Slot());
  database->SetSystemSlot(std::move(system_slot_copy));

  system_token_cert_database_ = std::move(database);

  VLOG(1) << "SystemTokenCertDBInitializer: Passing system token NSS "
             "database to NetworkCertLoader.";
  NetworkCertLoader::Get()->SetSystemNSSDB(system_token_cert_database_.get());
}

}  // namespace chromeos
