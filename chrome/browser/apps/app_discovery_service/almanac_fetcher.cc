// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/almanac_fetcher.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/almanac_api_client/almanac_icon_cache.h"
#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_discovery_service/launcher_app_almanac_endpoint.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace apps {
namespace {
// Relative file path to where the launcher app data will be stored on disk.
constexpr char kLauncherAppFilePath[] =
    "app_discovery_service/launcher_app/data.pb";
// Profile preference used to track the last time the Almanac was called for the
// Launcher App use case.
constexpr char kLastLauncherAppAlmanacCallTimestamp[] =
    "app_discovery_service.last_launcher_app_almanac_call_timestamp";

// Whether or not to skip the check if the build includes the Google Chrome API
// key. Used for testing.
bool skip_api_key_check_for_testing = false;

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
    // There should be just a single app with a single icon. We want to
    // handle more in the future but for now just read the first icon.
    const proto::LauncherAppResponse::Icon& icon = app_group.icons(0);
    auto extras = std::make_unique<GameExtras>(
        base::UTF8ToUTF16(app_group.badge_text()),
        /*relative_icon_path_=*/base::FilePath(""), icon.is_masking_allowed(),
        GURL(app_group.action_link()));

    apps.emplace_back(AppSource::kGames, icon.url(),
                      base::UTF8ToUTF16(app_group.name()), std::move(extras));
  }
  return apps;
}

// Handles the downloaded image and converts it to the right format.
void OnIconDownloaded(GetIconCallback callback, const gfx::Image& icon) {
  DiscoveryError status = DiscoveryError::kSuccess;
  if (icon.IsEmpty()) {
    status = DiscoveryError::kErrorRequestFailed;
  }
  std::move(callback).Run(icon.AsImageSkia(), status);
}
}  // namespace

AlmanacFetcher::AlmanacFetcher(Profile* profile,
                               std::unique_ptr<AlmanacIconCache> icon_cache)
    : profile_(profile),
      icon_cache_(std::move(icon_cache)) {
  // The whole feature would not work unless the build includes the Google
  // Chrome API key or this is a test environment as the server call would fail.
  if ((google_apis::IsGoogleChromeAPIKeyUsed() ||
       skip_api_key_check_for_testing) &&
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

// Method calls the icon cache for the given url.
void AlmanacFetcher::GetIcon(const std::string& icon_id,
                             int32_t size_hint_in_dip,
                             GetIconCallback callback) {
  // Do not use the icon cache if the environment isn't setup correctly.
  if (!icon_cache_ || apps_.empty()) {
    std::move(callback).Run(gfx::ImageSkia(),
                            DiscoveryError::kErrorRequestFailed);
    return;
  }
  // We ignore the size as it's hard-coded to kAppIconDimension in:
  // //chrome/browser/ash/app_list/search/common/icon_constants.h
  icon_cache_->GetIcon(GURL(icon_id),
                       base::BindOnce(&OnIconDownloaded, std::move(callback)));
}

void AlmanacFetcher::OnAppsUpdate(
    std::optional<proto::LauncherAppResponse> response) {
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

void AlmanacFetcher::SetSkipApiKeyCheckForTesting(bool skip_api_key_check) {
  skip_api_key_check_for_testing = skip_api_key_check;
}

void AlmanacFetcher::DownloadApps() {
  if ((base::Time::Now() - GetLastAppsUpdateTime()).InHours() >= 24) {
    launcher_app_almanac_endpoint::GetApps(
        profile_, base::BindOnce(&AlmanacFetcher::OnServerResponse,
                                 weak_factory_.GetWeakPtr()));
  } else {
    proto_file_manager_->ReadProtoFromFile(base::BindOnce(
        &AlmanacFetcher::OnAppsUpdate, weak_factory_.GetWeakPtr()));
  }
}

void AlmanacFetcher::OnServerResponse(
    std::optional<proto::LauncherAppResponse> response) {
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
