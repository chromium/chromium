// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVER_CONNECTOR_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVER_CONNECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace apps {

using GetDeduplicateAppsCallback =
    base::OnceCallback<void(absl::optional<proto::DeduplicateData>)>;

// The AppDeduplicationServerConnector is used to talk to the App Deduplication
// Service API endpoint in the Fondue server. Its role is to make requests and
// receive responses.
class AppDeduplicationServerConnector {
 public:
  AppDeduplicationServerConnector();
  AppDeduplicationServerConnector(const AppDeduplicationServerConnector&) =
      delete;
  AppDeduplicationServerConnector& operator=(
      const AppDeduplicationServerConnector&) = delete;
  ~AppDeduplicationServerConnector();

  // Fetches a list of duplicate app groups from the App Deduplication Service
  // endpoint in the Fondue server.
  void GetDeduplicateAppsFromServer(
      const DeviceInfo& device_info,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GetDeduplicateAppsCallback callback);

  // Returns the GURL for the App Deduplication Service endpoint. Exposed for
  // tests.
  static GURL GetServerUrl();

 private:
  // Processes the response from the request to the App Deduplication Service
  // endpoint and checks for errors.
  void OnGetDeduplicateAppsResponse(
      std::unique_ptr<network::SimpleURLLoader> loader,
      GetDeduplicateAppsCallback callback,
      std::unique_ptr<std::string> response_body);

  base::WeakPtrFactory<AppDeduplicationServerConnector> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVER_CONNECTOR_H_
