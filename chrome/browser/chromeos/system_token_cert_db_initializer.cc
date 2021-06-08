// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_token_cert_db_initializer.h"

#include <pk11pub.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/system_token_cert_db_storage.h"
#include "chromeos/tpm/buildflags.h"
#include "chromeos/tpm/tpm_token_loader.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util_internal.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/nss_cert_database_chromeos.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kInitialRequestDelay =
    base::TimeDelta::FromMilliseconds(100);
constexpr base::TimeDelta kMaxRequestDelay = base::TimeDelta::FromMinutes(5);

#if BUILDFLAG(SYSTEM_SLOT_SOFTWARE_FALLBACK)
constexpr bool kIsSystemSlotSoftwareFallbackAllowed = true;
#else
constexpr bool kIsSystemSlotSoftwareFallbackAllowed = false;
#endif

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
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
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

// Calculates the delay before running next attempt to get the TPM state
// (enabled/disabled), if |last_delay| was the last or initial delay.
base::TimeDelta GetNextRequestDelay(base::TimeDelta last_delay) {
  // This implements an exponential backoff, as we don't know in which order of
  // magnitude the TPM token changes it's state. The delay is capped to prevent
  // overflow. This threshold is arbitrarily chosen.
  return std::min(last_delay * 2, kMaxRequestDelay);
}

}  // namespace

constexpr base::TimeDelta
    SystemTokenCertDBInitializer::kMaxCertDbRetrievalDelay;

SystemTokenCertDBInitializer::SystemTokenCertDBInitializer()
    : tpm_request_delay_(kInitialRequestDelay),
      is_system_slot_software_fallback_allowed_(
          kIsSystemSlotSoftwareFallbackAllowed) {
  // Only start loading the system token once cryptohome is available and only
  // if the TPM is ready (available && owned && not being owned).
  UserDataAuthClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&SystemTokenCertDBInitializer::OnCryptohomeAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

SystemTokenCertDBInitializer::~SystemTokenCertDBInitializer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note that the observer could potentially not be added yet, but
  // the operation is a no-op in that case.
  TpmManagerClient::Get()->RemoveObserver(this);

  // Notify consumers of SystemTokenCertDbStorage that the database is not
  // usable anymore.
  SystemTokenCertDbStorage::Get()->ResetDatabase();

  // Destroy the NSSCertDatabase on the IO thread because consumers could be
  // accessing it there.
  content::GetIOThreadTaskRunner({})->DeleteSoon(
      FROM_HERE, std::move(system_token_cert_database_));
}

void SystemTokenCertDBInitializer::OnOwnershipTaken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MaybeStartInitializingDatabase();
}

void SystemTokenCertDBInitializer::OnCryptohomeAvailable(bool available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!available) {
    LOG(ERROR) << "SystemTokenCertDBInitializer: Failed to wait for "
                  "cryptohome to become available.";
    return;
  }

  VLOG(1) << "SystemTokenCertDBInitializer: Cryptohome available.";
  TpmManagerClient::Get()->AddObserver(this);

  CheckTpm();
}

void SystemTokenCertDBInitializer::CheckTpm() {
  TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
      ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
      base::BindOnce(&SystemTokenCertDBInitializer::OnGetTpmNonsensitiveStatus,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemTokenCertDBInitializer::RetryCheckTpmLater() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SystemTokenCertDBInitializer::CheckTpm,
                     weak_ptr_factory_.GetWeakPtr()),
      tpm_request_delay_);
  tpm_request_delay_ = GetNextRequestDelay(tpm_request_delay_);
}

void SystemTokenCertDBInitializer::OnGetTpmNonsensitiveStatus(
    const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reply.status() != ::tpm_manager::STATUS_SUCCESS) {
    LOG(WARNING) << "Failed to get tpm status; status: " << reply.status();
    RetryCheckTpmLater();
    return;
  }

  // There are 2 cases we start initializing the database at this point: 1. TPM
  // is ready, i.e., owned, or 2. TPM is disabled but software fallback is
  // allowed. Note that we don't fall back to software solution as long as TPM
  // is enabled.
  if (reply.is_owned() ||
      (!reply.is_enabled() && is_system_slot_software_fallback_allowed_)) {
    VLOG_IF(1, !reply.is_owned())
        << "Initializing database when TPM is not owned.";
    MaybeStartInitializingDatabase();
    return;
  }

  // If the TPM is enabled but not owned yet, request taking TPM initialization;
  // when it's done, the ownership taken signal triggers database
  // initialization.
  if (reply.is_enabled() && !reply.is_owned()) {
    VLOG(1) << "SystemTokenCertDBInitializer: TPM is not ready - not loading "
               "system token.";
    if (ShallAttemptTpmOwnership()) {
      // Requests tpm manager to initialize TPM, if it haven't done that yet.
      // The previous request from EULA dialogue could have been lost if
      // initialization was interrupted. We don't care about the result, and
      // don't block waiting for it.
      LOG(WARNING) << "Request taking TPM ownership.";
      TpmManagerClient::Get()->TakeOwnership(
          ::tpm_manager::TakeOwnershipRequest(), base::DoNothing());
    }
    return;
  }
}

void SystemTokenCertDBInitializer::MaybeStartInitializingDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (started_initializing_)
    return;
  started_initializing_ = true;
  VLOG(1)
      << "SystemTokenCertDBInitializer: TPM is ready, loading system token.";
  NetworkCertLoader::Get()->MarkSystemNSSDBWillBeInitialized();
  TPMTokenLoader::Get()->EnsureStarted();
  base::RepeatingCallback<void(crypto::ScopedPK11Slot)> callback =
      base::BindRepeating(&SystemTokenCertDBInitializer::InitializeDatabase,
                          weak_ptr_factory_.GetWeakPtr());
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&GetSystemSlotOnIOThread, callback));
}

void SystemTokenCertDBInitializer::InitializeDatabase(
    crypto::ScopedPK11Slot system_slot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

  auto* system_token_cert_db_storage = SystemTokenCertDbStorage::Get();
  DCHECK(system_token_cert_db_storage);
  system_token_cert_db_storage->SetDatabase(system_token_cert_database_.get());
}

}  // namespace chromeos
