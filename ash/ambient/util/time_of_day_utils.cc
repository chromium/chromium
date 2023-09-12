// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/time_of_day_utils.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace ash {

namespace {

constexpr base::FilePath::CharType kAssetsRootDir[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/personalization/time_of_day");
constexpr base::FilePath::CharType kSrcSubDir[] = FILE_PATH_LITERAL("src");

constexpr char kTimeOfDayDlcId[] = "timeofday-dlc";

void OnInstallDlcComplete(base::OnceCallback<void(base::FilePath)> on_done,
                          const DlcserviceClient::InstallResult& result) {
  CHECK(on_done);
  base::FilePath install_dir;
  if (result.error == dlcservice::kErrorNone) {
    install_dir = base::FilePath(result.root_path);
  } else {
    LOG(ERROR) << "Failed to install " << kTimeOfDayDlcId << " with error "
               << result.error;
  }
  std::move(on_done).Run(install_dir);
}

}  // namespace

const base::FilePath& GetTimeOfDaySrcDir() {
  static const base::NoDestructor<base::FilePath> kPath(
      base::FilePath(kAssetsRootDir).Append(kSrcSubDir));
  return *kPath;
}

void InstallTimeOfDayDlc(base::OnceCallback<void(base::FilePath)> on_done) {
  DlcserviceClient* client = DlcserviceClient::Get();
  CHECK(client);
  dlcservice::InstallRequest install_request;
  install_request.set_id(kTimeOfDayDlcId);
  client->Install(install_request,
                  base::BindOnce(&OnInstallDlcComplete, std::move(on_done)),
                  /*ProgressCallback=*/base::DoNothing());
}

const base::FilePath::CharType kTimeOfDayCloudsVideo[] =
    FILE_PATH_LITERAL("clouds.webm");
const base::FilePath::CharType kTimeOfDayNewMexicoVideo[] =
    FILE_PATH_LITERAL("new_mexico.webm");
const base::FilePath::CharType kAmbientVideoHtml[] =
    FILE_PATH_LITERAL("ambient_video.html");

}  // namespace ash
