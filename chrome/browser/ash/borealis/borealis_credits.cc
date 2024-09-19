// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_credits.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace borealis {

namespace {

const char kBorealisCreditsDlcSubpath[] = "credits.html";

std::string LoadCreditsFileBlocking(std::string dlc_root_path) {
  std::string contents;
  base::FilePath path =
      base::FilePath(dlc_root_path).Append(kBorealisCreditsDlcSubpath);
  if (!base::ReadFileToString(path, &contents)) {
    LOG(ERROR) << "Failed to load credits file: Failed to read " << path;
    return "";
  }
  return contents;
}

void OnStateQueried(base::OnceCallback<void(std::string)> callback,
                    std::string_view err,
                    const dlcservice::DlcState& state) {
  if (err != dlcservice::kErrorNone) {
    LOG(ERROR) << "Failed to load credits file: DLC error: " << err;
    std::move(callback).Run("");
    return;
  }
  if (state.state() != dlcservice::DlcState_State::DlcState_State_INSTALLED) {
    VLOG(1) << "Can't load credits file: DLC not available";
    std::move(callback).Run("");
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadCreditsFileBlocking, state.root_path()),
      std::move(callback));
}

}  // namespace

void LoadBorealisCredits(Profile* profile,
                         base::OnceCallback<void(std::string)> callback) {
  if (!BorealisServiceFactory::GetForProfile(profile)->Features().IsEnabled()) {
    VLOG(1) << "Can't load credits file: Borealis not installed";
    std::move(callback).Run("");
    return;
  }
  ash::DlcserviceClient::Get()->GetDlcState(
      kBorealisDlcName, base::BindOnce(&OnStateQueried, std::move(callback)));
}

}  // namespace borealis
