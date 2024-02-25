// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_data_remover.h"

#include <string>
#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"

namespace arc {

// The conversion of upstart job names to dbus object paths is undocumented. See
// function nih_dbus_path in libnih for the implementation.
constexpr char kArcRemoveDataUpstartJob[] = "arc_2dremove_2ddata";

ArcDataRemover::ArcDataRemover(PrefService* prefs,
                               const cryptohome::Identification& cryptohome_id)
    : cryptohome_id_(cryptohome_id) {
  pref_.Init(prefs::kArcDataRemoveRequested, prefs);
}

ArcDataRemover::~ArcDataRemover() = default;

void ArcDataRemover::Schedule() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  pref_.SetValue(true);
}

bool ArcDataRemover::IsScheduledForTesting() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return pref_.GetValue();
}

void ArcDataRemover::Run(RunCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!pref_.GetValue()) {
    VLOG(1) << "Data removal is not scheduled, skip.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  VLOG(1) << "Starting ARC data removal";
  auto* upstart_client = ash::UpstartClient::Get();
  if (!upstart_client) {
    // May be null in tests
    std::move(callback).Run(std::nullopt);
    return;
  }
  const std::string account_id =
      cryptohome::CreateAccountIdentifierFromIdentification(cryptohome_id_)
          .account_id();
  upstart_client->StartJob(
      kArcRemoveDataUpstartJob, {"CHROMEOS_USER=" + account_id},
      base::BindOnce(&ArcDataRemover::OnDataRemoved, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ArcDataRemover::OnDataRemoved(RunCallback callback, bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  UMA_HISTOGRAM_BOOLEAN("Arc.DataRemoved.Success", success);

  if (success) {
    VLOG(1) << "ARC data removal successful";
  } else {
    LOG(ERROR) << "Request for ARC user data removal failed. "
               << "See upstart logs for more details.";
  }
  pref_.SetValue(false);

  std::move(callback).Run(success);
}

}  // namespace arc
