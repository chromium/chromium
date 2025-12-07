// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_ALMANAC_ENDPOINT_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_ALMANAC_ENDPOINT_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/ash/login/oobe_apps_service/proto/oobe.pb.h"

class Profile;

namespace ash::oobe_apps_almanac_endpoint {

using GetAppsCallback =
    base::OnceCallback<void(std::optional<oobe::proto::OOBEListResponse>)>;

// Fetches a list of apps and use-cases from the endpoint in the Almanac
// server.
void GetAppsAndUseCases(Profile* profile, GetAppsCallback callback);

}  // namespace ash::oobe_apps_almanac_endpoint

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_ALMANAC_ENDPOINT_H_
