// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/almanac_fetcher.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/profiles/profile.h"

namespace apps {
namespace {

// Maps the Almanac Launcher App proto response to an app result. The icon
// information is not needed here as this is all handled by the icon cache.
std::vector<Result> MapToApps(const proto::LauncherAppResponse& proto) {
  std::vector<Result> apps;
  for (proto::LauncherAppResponse::AppGroup app_group : proto.app_groups()) {
    if (app_group.app_instances().empty()) {
      continue;
    }
    // There should be just a single GFN instance. We want to handle more
    // platforms in the future but for now just read the first one.
    const proto::LauncherAppResponse::AppInstance& instance =
        app_group.app_instances(0);
    auto extras = std::make_unique<GameExtras>(
        // TODO(b/296157719): construct this from the package id platform
        // instead of hardcoding.
        u"GeForce NOW",
        /*relative_icon_path_=*/base::FilePath(""),
        /*is_icon_masking_allowed_=*/false, GURL(instance.deeplink()));

    // TODO(b/296157719): use the package id instead.
    apps.emplace_back(AppSource::kGames, instance.app_id_for_platform(),
                      base::UTF8ToUTF16(app_group.name()), std::move(extras));
  }
  return apps;
}

}  // namespace

AlmanacFetcher::AlmanacFetcher(Profile* profile) : profile_(profile) {}

AlmanacFetcher::~AlmanacFetcher() = default;

void AlmanacFetcher::GetApps(ResultCallback callback) {
  auto error = apps_.empty() ? DiscoveryError::kErrorRequestFailed
                             : DiscoveryError::kSuccess;
  std::move(callback).Run(apps_, error);
}

base::CallbackListSubscription AlmanacFetcher::RegisterForAppUpdates(
    RepeatingResultCallback callback) {
  return subscribers_.Add(std::move(callback));
}

// Method does nothing as all icons are handled by the icon cache.
void AlmanacFetcher::GetIcon(const std::string& app_id,
                             int32_t size_hint_in_dip,
                             GetIconCallback callback) {}

void AlmanacFetcher::OnAppsUpdate(
    const proto::LauncherAppResponse& launcher_app_response) {
  apps_ = MapToApps(launcher_app_response);
  subscribers_.Notify(apps_);
}

}  // namespace apps
