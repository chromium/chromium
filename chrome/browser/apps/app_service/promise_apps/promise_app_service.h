// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_SERVICE_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_icon_cache.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_effects.h"

class Profile;

namespace gfx {
class Image;
}
namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace apps {

struct PromiseApp;
struct IconValue;

class PromiseAppIcon;
class PromiseAppIconCache;

class PackageId;
class PromiseAppAlmanacConnector;
class PromiseAppRegistryCache;
class PromiseAppWrapper;

using PromiseAppPtr = std::unique_ptr<PromiseApp>;
using PromiseAppIconPtr = std::unique_ptr<PromiseAppIcon>;
using IconValuePtr = std::unique_ptr<IconValue>;

using IconDownloadedCallback =
    base::OnceCallback<void(const gfx::Image& image,
                            const image_fetcher::RequestMetadata& metadata)>;

// This service is responsible for registering and managing promise apps,
// including retrieving any data required to populate a promise app object.
// These promise apps will result in a "promise icon" that the user sees in the
// Launcher/ Shelf, which represents a pending or active app installation.
class PromiseAppService : public AppRegistryCache::Observer {
 public:
  explicit PromiseAppService(Profile* profile,
                             AppRegistryCache& app_registry_cache);

  PromiseAppService(const PromiseAppService&) = delete;
  PromiseAppService& operator=(const PromiseAppService&) = delete;
  ~PromiseAppService() override;

  apps::PromiseAppRegistryCache* PromiseAppRegistryCache();

  apps::PromiseAppIconCache* PromiseAppIconCache();

  // Adds or updates a promise app in the Promise App Registry Cache with the
  // fields provided in `delta`. For new promise app registrations, we send a
  // request to the Almanac API to retrieve additional promise app info.
  void OnPromiseApp(PromiseAppPtr delta);

  // Retrieves the icon for a package ID and applies any specified effects.
  void LoadIcon(const PackageId& package_id,
                int32_t size_hint_in_dip,
                apps::IconEffects icon_effects,
                apps::LoadIconCallback callback);

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  void OnApkWebAppInstallationFinished(const std::string& package_name);

  // Allows us to skip Almanac implementation when running unit tests that don't
  // care about Almanac responses.
  void SetSkipAlmanacForTesting(bool skip_almanac);

  // Allows tests to trigger an Almanac query without needing an official Google
  // API key.
  void SetSkipApiKeyCheckForTesting(bool skip_api_key_check);

  // Tries to update install priroity if possible for ARC apps.
  void UpdateInstallPriority(const std::string& id);

 private:
  // Update a promise app's fields with the info retrieved from the Almanac API.
  void OnGetPromiseAppInfoCompleted(
      const PackageId& package_id,
      std::optional<PromiseAppWrapper> promise_app_info);

  // Adds an icon to the icon cache and marks the corresponding promise app
  // as ready to show after all the icons are downloaded.
  void OnIconDownloaded(const PackageId& package_id,
                        const gfx::Image& image,
                        const image_fetcher::RequestMetadata& metadata);

  // Check whether there is a registered app in AppRegistryCache with the
  // specified package ID.
  bool IsRegisteredInAppRegistryCache(const PackageId& package_id);

  // Set `should_show` to true for a promise app.
  void SetPromiseAppReadyToShow(const PackageId& package_id);

  raw_ptr<Profile> profile_;

  // The cache that contains all the promise apps in the system.
  std::unique_ptr<apps::PromiseAppRegistryCache> promise_app_registry_cache_;

  // Retrieves information from the Almanac Promise App API about the
  // packages being installed.
  std::unique_ptr<PromiseAppAlmanacConnector> promise_app_almanac_connector_;

  // Cache that contains all promise app icons.
  std::unique_ptr<apps::PromiseAppIconCache> promise_app_icon_cache_;

  // Fetches images from a given URL.
  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  raw_ptr<apps::AppRegistryCache> app_registry_cache_;

  // Keeps track of how many icon downloads we are waiting on for each promise
  // app. When all downloads are completed, we can proceed to set (or not set)
  // the promise app as ready to show to the user.
  std::map<PackageId, int> pending_download_count_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observation_{this};

  bool skip_almanac_for_testing_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PromiseAppService> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_SERVICE_H_
