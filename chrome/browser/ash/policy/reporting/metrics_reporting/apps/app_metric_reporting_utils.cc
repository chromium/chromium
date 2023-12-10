// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_metric_reporting_utils.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace reporting {

std::optional<std::string> GetPublisherIdForApp(const std::string& app_id,
                                                Profile* profile) {
  CHECK(profile);
  CHECK(
      ::apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  std::optional<std::string> publisher_id = std::nullopt;
  ::apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&publisher_id](const ::apps::AppUpdate& app_update) {
        if (!app_update.PublisherId().empty()) {
          publisher_id = app_update.PublisherId();
        }
      });
  return publisher_id;
}

}  // namespace reporting
