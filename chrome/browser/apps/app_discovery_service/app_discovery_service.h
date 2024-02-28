// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_SERVICE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace apps {

class AppFetcherManager;

// API for consumers to use to fetch apps.
class AppDiscoveryService : public KeyedService {
 public:
  explicit AppDiscoveryService(Profile* profile);
  AppDiscoveryService(const AppDiscoveryService&) = delete;
  AppDiscoveryService& operator=(const AppDiscoveryService&) = delete;
  ~AppDiscoveryService() override;

  // Returns a valid CallbackListSubscription if the supplied |result_type| can
  // handle app updates. After successful registration, each time updates to
  // data of |result_type| occurs, |callback| will be called.
  //
  // Clients should check the returned callback is alive and
  // save the returned value as a member variable to ensure correct lifecycle
  // management.
  base::CallbackListSubscription RegisterForAppUpdates(
      ResultType result_type,
      RepeatingResultCallback callback);

  // Queries for apps of the requested |result_type|.
  // |callback| is called when a response to the request is ready.
  void GetApps(ResultType result_type, ResultCallback callback);

  // Queries for an app's icon, identified by |icon_id|.
  // |callback| is called when a response to the request is ready.
  // Virtual for testing.
  virtual void GetIcon(const std::string& icon_id,
                       int32_t size_hint_in_dip,
                       ResultType result_type,
                       GetIconCallback callback);

 private:
  std::unique_ptr<AppFetcherManager> app_fetcher_manager_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_SERVICE_H_
