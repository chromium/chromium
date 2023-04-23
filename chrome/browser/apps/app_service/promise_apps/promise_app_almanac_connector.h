// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_ALMANAC_CONNECTOR_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_ALMANAC_CONNECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

class Profile;
namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace apps {

class PromiseAppWrapper;

using GetPromiseAppCallback =
    base::OnceCallback<void(absl::optional<PromiseAppWrapper>)>;

// The PromiseAppAlmanacConnector talks to the Almanac Promise App
// API endpoint. Its role is to build requests and convert responses into
// usable objects.
class PromiseAppAlmanacConnector {
 public:
  explicit PromiseAppAlmanacConnector(Profile* profile);
  PromiseAppAlmanacConnector(const PromiseAppAlmanacConnector&) = delete;
  PromiseAppAlmanacConnector& operator=(const PromiseAppAlmanacConnector&) =
      delete;
  ~PromiseAppAlmanacConnector();

  // Fetches app info from the Almanac Promise App Service API. `callback` will
  // be called with a single promise app, or `absl::nullopt` if an error
  // occurred while fetching apps.
  void GetPromiseAppInfo(const PackageId& package_id,
                         GetPromiseAppCallback callback);

  // Returns the URL for the Almanac Promise App endpoint. Exposed for tests.
  static GURL GetServerUrl();

 private:
  void GetPromiseAppInfoImpl(const PackageId& package_id,
                             GetPromiseAppCallback callback);

  // Set the device locale and continue to retrieve promise app info from
  // Almanac.
  void SetLocale(const PackageId& package_id,
                 GetPromiseAppCallback callback,
                 DeviceInfo device_info);

  std::string BuildGetPromiseAppRequestBody(const apps::PackageId& package_id);

  // Parse the response from the Almanac API and run the callback.
  void OnGetPromiseAppResponse(const PackageId& package_id,
                               std::unique_ptr<network::SimpleURLLoader> loader,
                               GetPromiseAppCallback callback,
                               std::unique_ptr<std::string> response_body);

  std::string locale_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<DeviceInfoManager> device_info_manager_;
  base::WeakPtrFactory<PromiseAppAlmanacConnector> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_ALMANAC_CONNECTOR_H_
