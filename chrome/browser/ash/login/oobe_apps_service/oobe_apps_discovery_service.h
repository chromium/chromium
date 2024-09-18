// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_DISCOVERY_SERVICE_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_DISCOVERY_SERVICE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_types.h"
#include "chrome/browser/ash/login/oobe_apps_service/proto/oobe.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class Profile;

namespace ash {

enum class AppsFetchingResult {
  kSuccess,             // Successfully got app data to return.
  kErrorRequestFailed,  // Failed to get requested data.
  kErrorMissingKey,     // Failed to get data due to missing keys.
  kMaxValue = kErrorMissingKey,
};

using ResultCallbackAppsAndUseCases =
    base::OnceCallback<void(const std::vector<OOBEAppDefinition>& apps,
                            const std::vector<OOBEDeviceUseCase>& usecases,
                            AppsFetchingResult error)>;

// API for consumers to use to fetch apps.
class OobeAppsDiscoveryService : public KeyedService {
 public:
  explicit OobeAppsDiscoveryService(Profile* profile);
  OobeAppsDiscoveryService(const OobeAppsDiscoveryService&) = delete;
  OobeAppsDiscoveryService& operator=(const OobeAppsDiscoveryService&) = delete;
  ~OobeAppsDiscoveryService() override;

  // Queries for apps and use-cases for the endpoint.
  virtual void GetAppsAndUseCases(ResultCallbackAppsAndUseCases callback);

 private:
  // Downloads apps and use-cases from the server.
  // updates the in-memory apps and usecases list.
  void DownloadAppsAndUseCases();

  void PropagateResult(ResultCallbackAppsAndUseCases callback,
                       AppsFetchingResult result);

  // Calls the Almanac server with the device information provided.
  void OnGetDeviceInfo(apps::DeviceInfo device_info);

  // Writes the response to the result apps and categories list.
  void OnServerResponse(std::optional<oobe::proto::OOBEListResponse> response);

  raw_ptr<Profile> profile_;
  std::vector<OOBEAppDefinition> apps_list_;
  std::vector<OOBEDeviceUseCase> use_cases_;
  ResultCallbackAppsAndUseCases callback_;

  base::WeakPtrFactory<OobeAppsDiscoveryService> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_DISCOVERY_SERVICE_H_
