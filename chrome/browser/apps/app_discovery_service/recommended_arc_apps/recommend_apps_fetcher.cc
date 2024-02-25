// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher.h"

#include "ash/constants/ash_switches.h"
#include "ash/public/ash_interfaces.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/fake_recommend_apps_fetcher.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace apps {
namespace {

// The factory callback that will be used to create RecommendAppsFetcher
// instances other than default RecommendAppsFetcherImpl.
// It can be set by SetFactoryCallbackForTesting().
RecommendAppsFetcher::FactoryCallback* g_factory_callback = nullptr;

}  // namespace

// static
std::unique_ptr<RecommendAppsFetcher> RecommendAppsFetcher::Create(
    Profile* profile,
    RecommendAppsFetcherDelegate* delegate) {
  if (g_factory_callback) {
    return g_factory_callback->Run(delegate);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kFakeArcRecommendedAppsForTesting)) {
    LOG(WARNING) << "Using fake recommended apps fetcher";
    std::string num_apps_str =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            ash::switches::kFakeArcRecommendedAppsForTesting);
    int fake_apps_count = 0;
    if (!base::StringToInt(num_apps_str, &fake_apps_count)) {
      fake_apps_count = 3;
    }
    return std::make_unique<FakeRecommendAppsFetcher>(delegate,
                                                      fake_apps_count);
  }

  mojo::PendingRemote<crosapi::mojom::CrosDisplayConfigController>
      display_config;
  ash::BindCrosDisplayConfigController(
      display_config.InitWithNewPipeAndPassReceiver());
  return std::make_unique<RecommendAppsFetcherImpl>(
      delegate, std::move(display_config),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
}

// static
void RecommendAppsFetcher::SetFactoryCallbackForTesting(
    FactoryCallback* callback) {
  DCHECK(!g_factory_callback || !callback);

  g_factory_callback = callback;
}

}  // namespace apps
