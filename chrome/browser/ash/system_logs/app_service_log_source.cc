// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/app_service_log_source.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

constexpr char kAppServiceLogEntry[] = "app_service";

}  // namespace

AppServiceLogSource::AppServiceLogSource()
    : SystemLogsSource("AppServiceLog") {}

AppServiceLogSource::~AppServiceLogSource() {}

void AppServiceLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();

  const auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user) {
    std::move(callback).Run(std::move(response));
    return;
  }
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile ||
      !apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    std::move(callback).Run(std::move(response));
    return;
  }

  std::set<std::string> running;
  std::set<std::string> seen;
  std::stringstream log_data;

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->InstanceRegistry().ForEachInstance(
      [&running](const apps::InstanceUpdate& update) {
        running.insert(update.AppId());
      });
  proxy->AppRegistryCache().ForEachApp([&log_data, &running, &seen, profile](
                                           const apps::AppUpdate& update) {
    std::string status =
        base::Contains(running, update.AppId()) ? "running" : "installed";
    auto app_type = apps::GetAppTypeHistogramName(
        apps::GetAppTypeName(profile, update.AppType(), update.AppId(),
                             apps::LaunchContainer::kLaunchContainerNone));
    std::string id =
        apps::AppPlatformMetrics::GetURLForApp(profile, update.AppId()).spec();
    // Different apps can coalesce to the same ID, only report the first
    // instance.
    if (id.empty() || base::Contains(seen, id)) {
      return;
    }
    log_data << id << ", " << app_type << ", " << status << std::endl;
    seen.insert(id);
  });

  (*response)[kAppServiceLogEntry] = log_data.str();
  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
