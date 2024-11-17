// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_terminal_provider.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/extensions/api/terminal/startup_status.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace bruschetta {

BruschettaTerminalProvider::BruschettaTerminalProvider(
    Profile* profile,
    guest_os::GuestId guest_id)
    : profile_(profile), guest_id_(std::move(guest_id)) {}
BruschettaTerminalProvider::~BruschettaTerminalProvider() = default;

std::string BruschettaTerminalProvider::Label() {
  return GetDisplayName(profile_, guest_id_);
}

guest_os::GuestId BruschettaTerminalProvider::GuestId() {
  return guest_id_;
}

bool BruschettaTerminalProvider::RecoveryRequired(int64_t display_id) {
  return false;
}

bool BruschettaTerminalProvider::AllowedByPolicy() {
  auto config = GetConfigForGuest(profile_, guest_id_,
                                  prefs::PolicyEnabledState::RUN_ALLOWED);

  return config.has_value() && config.value();
}

std::string BruschettaTerminalProvider::PrepareCwd(
    storage::FileSystemURL path) {
  LOG(ERROR) << "Bruschetta terminal doesn't support arbitrary shared "
                "folders as working directories, opening at root instead";
  return "";
}

std::unique_ptr<extensions::StartupStatus>
BruschettaTerminalProvider::CreateStartupStatus(
    std::unique_ptr<extensions::StartupStatusPrinter> printer) {
  // No custom Bruschetta startup status yet, so just use the default.
  return std::make_unique<extensions::StartupStatus>(std::move(printer), 2);
}

void BruschettaTerminalProvider::EnsureRunning(
    extensions::StartupStatus* startup_status,
    base::OnceCallback<void(bool success, std::string failure_reason)>
        callback) {
  // TODO(b/231390254): Bruschetta doesn't support observing the steps during
  // launch, so for now we just have a single "starting vm" step reusing a
  // Crostini string.
  startup_status->printer()->PrintStage(
      1,
      l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_STATUS_START_TERMINA_VM));
  auto launcher =
      BruschettaServiceFactory::GetForProfile(profile_)->GetLauncher(
          guest_id_.vm_name);

  if (launcher) {
    launcher->EnsureRunning(base::BindOnce(
        [](base::OnceCallback<void(bool, std::string)> callback,
           BruschettaResult result) {
          bool success = (result == BruschettaResult::kSuccess);
          std::move(callback).Run(
              success,
              success ? ""
                      : base::StringPrintf(
                            "Error starting bruschetta for terminal: %d (%s)",
                            static_cast<int>(result),
                            BruschettaResultString(result)));
        },
        std::move(callback)));
  } else {
    std::move(callback).Run(
        false,
        base::StringPrintf("Bruschetta VM %s unknown or disabled by policy",
                           guest_id_.vm_name.c_str()));
  }
}

}  // namespace bruschetta
