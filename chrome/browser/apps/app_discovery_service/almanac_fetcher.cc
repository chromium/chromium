// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/almanac_fetcher.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_discovery_service/launcher_app_almanac_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace apps {
namespace {
// Relative file path to where the launcher app data will be stored on disk.
constexpr char kLauncherAppFilePath[] =
    "app_discovery_service/launcher_app/data.pb";
// Profile preference used to track the last time the Almanac was called for the
// Launcher App use case.
constexpr char kLastLauncherAppAlmanacCallTimestamp[] =
    "app_discovery_service.last_launcher_app_almanac_call_timestamp";

// Maps the Almanac Launcher App proto response to an app result. The icon
// url is passed and later handled by the icon cache.
std::vector<Result> MapToApps(const proto::LauncherAppResponse& proto) {
  std::vector<Result> apps;
  for (proto::LauncherAppResponse::AppGroup app_group : proto.app_groups()) {
    // Skip apps we cannot display.
    if (app_group.icons().empty() || app_group.name().empty() ||
        app_group.action_link().empty()) {
      continue;
    }
    // There should be just a single GFN app with a single icon. We want to
    // handle more in the future but for now just read the first icon.
    const proto::LauncherAppResponse::Icon& icon = app_group.icons(0);
    auto extras = std::make_unique<GameExtras>(
        u"GeForce NOW",
        /*relative_icon_path_=*/base::FilePath(""), icon.is_masking_allowed(),
        GURL(app_group.action_link()));

    apps.emplace_back(AppSource::kGames, icon.url(),
                      base::UTF8ToUTF16(app_group.name()), std::move(extras));
  }
  return apps;
}

}  // namespace

AlmanacFetcher::AlmanacFetcher(Profile* profile)
    : profile_(profile),
      server_connector_(std::make_unique<LauncherAppAlmanacConnector>()),
      device_info_manager_(std::make_unique<DeviceInfoManager>(profile)) {
  // TODO(b/296157719): add an API key check.
  if (base::FeatureList::IsEnabled(kAlmanacGameMigration) &&
      chromeos::features::IsCloudGamingDeviceEnabled()) {
    base::FilePath path = profile->GetPath().AppendASCII(kLauncherAppFilePath);
    proto_file_manager_ =
        std::make_unique<ProtoFileManager<proto::LauncherAppResponse>>(path);
    DownloadApps();
  }
}

AlmanacFetcher::~AlmanacFetcher() = default;

void AlmanacFetcher::GetApps(ResultCallback callback) {
  auto error = apps_.empty() ? DiscoveryError::kErrorRequestFailed
                             : DiscoveryError::kSuccess;
  std::move(callback).Run(apps_, error);
}

base::CallbackListSubscription AlmanacFetcher::RegisterForAppUpdates(
    RepeatingResultCallback callback) {
  if (!apps_.empty()) {
    callback.Run(apps_);
  }
  return subscribers_.Add(std::move(callback));
}

// Method does nothing as all icons are handled by the icon cache.
void AlmanacFetcher::GetIcon(const std::string& app_id,
                             int32_t size_hint_in_dip,
                             GetIconCallback callback) {}

void AlmanacFetcher::OnAppsUpdate(
    absl::optional<proto::LauncherAppResponse> response) {
  if (!response.has_value()) {
    return;
  }
  apps_ = MapToApps(*response);
  subscribers_.Notify(apps_);
}

void AlmanacFetcher::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(kLastLauncherAppAlmanacCallTimestamp,
                             base::Time());
}

void AlmanacFetcher::DownloadApps() {
  if ((base::Time::Now() - GetLastAppsUpdateTime()).InHours() >= 24) {
    device_info_manager_->GetDeviceInfo(base::BindOnce(
        &AlmanacFetcher::OnGetDeviceInfo, weak_factory_.GetWeakPtr()));
  } else {
    proto_file_manager_->ReadProtoFromFile(base::BindOnce(
        &AlmanacFetcher::OnAppsUpdate, weak_factory_.GetWeakPtr()));
  }
}

void AlmanacFetcher::OnGetDeviceInfo(DeviceInfo device_info) {
  server_connector_->GetApps(device_info, profile_->GetURLLoaderFactory(),
                             base::BindOnce(&AlmanacFetcher::OnServerResponse,
                                            weak_factory_.GetWeakPtr()));
}

void AlmanacFetcher::OnServerResponse(
    absl::optional<proto::LauncherAppResponse> response) {
  if (response.has_value()) {
    proto_file_manager_->WriteProtoToFile(
        *response, base::BindOnce(&AlmanacFetcher::OnFileWritten,
                                  weak_factory_.GetWeakPtr(), *response));
  } else {
    proto_file_manager_->ReadProtoFromFile(base::BindOnce(
        &AlmanacFetcher::OnAppsUpdate, weak_factory_.GetWeakPtr()));
  }
}

void AlmanacFetcher::OnFileWritten(proto::LauncherAppResponse response,
                                   bool write_complete) {
  OnAppsUpdate(std::move(response));
  if (!write_complete) {
    LOG(ERROR) << "Writing server response to disk failed";
    return;
  }
  // Only set if writing data to disk succeeded.
  SetLastAppsUpdateTime(base::Time::Now());
}

base::Time AlmanacFetcher::GetLastAppsUpdateTime() const {
  return profile_->GetPrefs()->GetTime(kLastLauncherAppAlmanacCallTimestamp);
}

void AlmanacFetcher::SetLastAppsUpdateTime(base::Time value) {
  profile_->GetPrefs()->SetTime(kLastLauncherAppAlmanacCallTimestamp, value);
}
}  // namespace apps
