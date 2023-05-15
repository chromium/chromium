// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace apps {

struct PromiseApp;
using PromiseAppPtr = std::unique_ptr<PromiseApp>;
class PackageId;
class PromiseAppAlmanacConnector;
class PromiseAppRegistryCache;
class PromiseAppWrapper;

// This service is responsible for registering and managing promise apps,
// including retrieving any data required to populate a promise app object.
// These promise apps will result in a "promise icon" that the user sees in the
// Launcher/ Shelf, which represents a pending or active app installation.
class PromiseAppService {
 public:
  explicit PromiseAppService(Profile* profile);

  PromiseAppService(const PromiseAppService&) = delete;
  PromiseAppService& operator=(const PromiseAppService&) = delete;
  ~PromiseAppService();

  apps::PromiseAppRegistryCache* PromiseAppRegistryCache();

  // Adds or updates a promise app in the Promise App Registry Cache with the
  // fields provided in `delta`. For new promise app registrations, we send a
  // request to the Almanac API to retrieve additional promise app info.
  void OnPromiseApp(PromiseAppPtr delta);

  // Allows us to skip Almanac implementation when running unit tests that don't
  // care about Almanac responses.
  void SetSkipAlmanacForTesting(bool skip_almanac);

 private:
  // Update a promise app's fields with the info retrieved from the Almanac API.
  void OnGetPromiseAppInfoCompleted(
      const PackageId& package_id,
      absl::optional<PromiseAppWrapper> promise_app_info);

  // The cache that contains all the promise apps in the system.
  std::unique_ptr<apps::PromiseAppRegistryCache> promise_app_registry_cache_;

  // Retrieves information from the Almanac Promise App API about the
  // packages being installed.
  std::unique_ptr<PromiseAppAlmanacConnector> promise_app_almanac_connector_;

  bool skip_almanac_for_testing_ = false;

  base::WeakPtrFactory<PromiseAppService> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_SERVICE_H_
