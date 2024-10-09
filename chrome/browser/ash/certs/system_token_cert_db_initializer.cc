// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/certs/system_token_cert_db_initializer.h"

#include <pk11pub.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/ash/components/tpm/buildflags.h"
#include "chromeos/ash/components/tpm/tpm_token_loader.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util_internal.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/nss_cert_database_chromeos.h"

namespace ash {

namespace {

constexpr base::TimeDelta kInitialRequestDelay = base::Milliseconds(100);
constexpr base::TimeDelta kMaxRequestDelay = base::Minutes(5);

#if BUILDFLAG(NSS_SLOTS_SOFTWARE_FALLBACK)
constexpr bool kIsSystemSlotSoftwareFallbackAllowed = true;
#else
constexpr bool kIsSystemSlotSoftwareFallbackAllowed = false;
#endif

// Called on IO Thread when the system slot has been retrieved.
void GotSystemSlotOnIOThread(
    base::OnceCallback<void(crypto::ScopedPK11Slot)> ui_callback,
    crypto::ScopedPK11Slot system_slot) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(ui_callback), std::move(system_slot)));
}

// Called on IO Thread, initiates retrieval of system slot. |ui_callback|
// will be executed on the UI thread when the system slot has been retrieved.
void GetSystemSlotOnIOThread(
    base::OnceCallback<void(crypto::ScopedPK11Slot)> ui_callback) {
  crypto::GetSystemNSSKeySlot(
      base::BindOnce(&GotSystemSlotOnIOThread, std::move(ui_callback)));
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
      is_nss_slots_software_fallback_allowed_(
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
  chromeos::TpmManagerClient::Get()->RemoveObserver(this);

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
  chromeos::TpmManagerClient::Get()->AddObserver(this);

  CheckTpm();
}

void SystemTokenCertDBInitializer::CheckTpm() {
  chromeos::TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
      ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
      base::BindOnce(&SystemTokenCertDBInitializer::OnGetTpmNonsensitiveStatus,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemTokenCertDBInitializer::RetryCheckTpmLater() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
      (!reply.is_enabled() && is_nss_slots_software_fallback_allowed_)) {
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
      chromeos::TpmManagerClient::Get()->TakeOwnership(
          ::tpm_manager::TakeOwnershipRequest(), base::DoNothing());
    }
    return;
  }
}

void SystemTokenCertDBInitializer::MaybeStartInitializingDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (started_initializing_) {
    return;
  }
  started_initializing_ = true;
  VLOG(1)
      << "SystemTokenCertDBInitializer: TPM is ready, loading system token.";
  NetworkCertLoader::Get()->MarkSystemNSSDBWillBeInitialized();
  TPMTokenLoader::Get()->EnsureStarted();
  auto ui_callback =
      base::BindOnce(&SystemTokenCertDBInitializer::InitializeDatabase,
                     weak_ptr_factory_.GetWeakPtr());
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetSystemSlotOnIOThread, std::move(ui_callback)));
}

void SystemTokenCertDBInitializer::InitializeDatabase(
    crypto::ScopedPK11Slot system_slot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!system_slot) {
    // System slot will never be loaded.
    return;
  }

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

}  // namespace ash
