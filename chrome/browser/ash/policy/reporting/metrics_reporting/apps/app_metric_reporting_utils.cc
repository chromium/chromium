// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_metric_reporting_utils.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_service.h"
#include "components/services/app_service/public/cpp/app_service_registry.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace reporting {

std::optional<std::string> GetPublisherIdForApp(const std::string& app_id,
                                                Profile* profile) {
  CHECK(profile);
  const AccountId& account_id =
      CHECK_DEREF(ash::AnnotatedAccountId::Get(profile));
  ::apps::AppService& app_service =
      CHECK_DEREF(::apps::AppServiceRegistry::Get()->Find(account_id));
  std::optional<std::string> publisher_id;
  app_service.AppRegistryCache().ForOneApp(
      app_id, [&publisher_id](const ::apps::AppUpdate& app_update) {
        if (!app_update.PublisherId().empty()) {
          publisher_id = app_update.PublisherId();
        }
      });
  return publisher_id;
}

}  // namespace reporting
