// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/app_info_generator.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/session_manager/core/session_manager.h"

namespace em = enterprise_management;

namespace {

em::AppInfo::Status ExtractStatus(const apps::mojom::Readiness readiness) {
  switch (readiness) {
    case apps::mojom::Readiness::kReady:
      return em::AppInfo::Status::AppInfo_Status_STATUS_INSTALLED;
    case apps::mojom::Readiness::kRemoved:
    case apps::mojom::Readiness::kUninstalledByUser:
      return em::AppInfo::Status::AppInfo_Status_STATUS_UNINSTALLED;
    case apps::mojom::Readiness::kDisabledByBlocklist:
    case apps::mojom::Readiness::kDisabledByPolicy:
    case apps::mojom::Readiness::kDisabledByUser:
    case apps::mojom::Readiness::kTerminated:
      return em::AppInfo::Status::AppInfo_Status_STATUS_DISABLED;
    case apps::mojom::Readiness::kUnknown:
      return em::AppInfo::Status::AppInfo_Status_STATUS_UNKNOWN;
  }
}

em::AppInfo::AppType ExtractAppType(const apps::mojom::AppType app_type) {
  switch (app_type) {
    case apps::mojom::AppType::kArc:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_ARC;
    case apps::mojom::AppType::kBuiltIn:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_BUILTIN;
    case apps::mojom::AppType::kCrostini:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_CROSTINI;
    case apps::mojom::AppType::kPluginVm:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_PLUGINVM;
    case apps::mojom::AppType::kExtension:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_EXTENSION;
    case apps::mojom::AppType::kWeb:
    case apps::mojom::AppType::kSystemWeb:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_WEB;
    case apps::mojom::AppType::kBorealis:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_BOREALIS;
    case apps::mojom::AppType::kMacOs:
    case apps::mojom::AppType::kLacros:
    case apps::mojom::AppType::kRemote:
    case apps::mojom::AppType::kUnknown:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_UNKNOWN;
  }
}

}  // namespace

namespace policy {

AppInfoGenerator::AppInfoProvider::AppInfoProvider(Profile* profile)
    : activity_storage(profile->GetPrefs(),
                       prefs::kAppActivityTimes,
                       /*day_start_offset=*/base::TimeDelta::FromSeconds(0)),
      app_service_proxy(*apps::AppServiceProxyFactory::GetForProfile(profile)),
      web_app_provider(
          *web_app::WebAppProviderFactory::GetForProfile(profile)) {}

AppInfoGenerator::AppInfoProvider::~AppInfoProvider() = default;

AppInfoGenerator::AppInfoGenerator(
    base::TimeDelta max_stored_past_activity_interval,
    base::Clock* clock)
    : max_stored_past_activity_interval_(max_stored_past_activity_interval),
      clock_(*clock) {}

AppInfoGenerator::AppInstances::AppInstances(const base::Time start_time_)
    : start_time(start_time_) {}

AppInfoGenerator::AppInstances::~AppInstances() = default;

AppInfoGenerator::~AppInfoGenerator() {
  SetOpenDurationsToClosed(clock_.Now());
}

// static
void AppInfoGenerator::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kAppActivityTimes);
}

const AppInfoGenerator::Result AppInfoGenerator::Generate() const {
  if (!should_report_) {
    VLOG(1) << "App usage reporting is not enabled for this user.";
    return base::nullopt;
  }
  if (!provider_) {
    VLOG(1) << "No affiliated user session. Returning empty app list.";
    return base::nullopt;
  }
  auto activity_periods = provider_->activity_storage.GetActivityPeriods();
  auto activity_compare = [](const em::TimePeriod& time_period1,
                             const em::TimePeriod& time_period2) {
    return time_period1.start_timestamp() < time_period2.start_timestamp();
  };
  std::vector<em::AppInfo> app_infos;
  provider_->app_service_proxy.AppRegistryCache().ForEachApp(
      [&app_infos, &activity_periods, &activity_compare,
       this](const apps::AppUpdate& update) {
        ActivityStorage::Activities& app_activity =
            activity_periods[update.AppId()];
        std::sort(app_activity.begin(), app_activity.end(), activity_compare);
        app_infos.push_back(ConvertToAppInfo(update, app_activity));
      });
  return app_infos;
}

void AppInfoGenerator::OnReportingChanged(bool should_report) {
  if (should_report_ == should_report) {
    return;
  }
  should_report_ = should_report;
  if (provider_) {
    if (should_report) {
      provider_->app_service_proxy.InstanceRegistry().AddObserver(this);
    } else {
      provider_->app_service_proxy.InstanceRegistry().RemoveObserver(this);
    }
  }
}

void AppInfoGenerator::OnReportedSuccessfully(const base::Time report_time) {
  if (!provider_) {
    return;
  }
  provider_->activity_storage.TrimActivityPeriods(
      report_time.ToJavaTime(), base::Time::Max().ToJavaTime());
}

void AppInfoGenerator::OnWillReport() {
  if (!provider_) {
    return;
  }
  SetOpenDurationsToClosed(clock_.Now());
  SetIdleDurationsToOpen();
}

void AppInfoGenerator::OnAffiliatedLogin(Profile* profile) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    VLOG(1) << "No apps available. Will not track usage.";
    return;
  }

  provider_ = std::make_unique<AppInfoGenerator::AppInfoProvider>(profile);
  provider_->activity_storage.PruneActivityPeriods(
      clock_.Now(), max_stored_past_activity_interval_);

  if (should_report_) {
    provider_->app_service_proxy.InstanceRegistry().AddObserver(this);
  }
}

void AppInfoGenerator::OnAffiliatedLogout(Profile* profile) {
  if (provider_) {
    if (should_report_) {
      provider_->app_service_proxy.InstanceRegistry().RemoveObserver(this);
    }
    provider_.reset();
  }
}

void AppInfoGenerator::OnLocked() {
  SetOpenDurationsToClosed(clock_.Now());
}

void AppInfoGenerator::OnUnlocked() {
  SetIdleDurationsToOpen();
}

void AppInfoGenerator::OnResumeActive(base::Time suspend_time) {
  SetOpenDurationsToClosed(suspend_time);
  SetIdleDurationsToOpen();
}

const em::AppInfo AppInfoGenerator::ConvertToAppInfo(
    const apps::AppUpdate& update,
    const std::vector<em::TimePeriod>& app_activity) const {
  em::AppInfo info;
  bool is_web_app = update.AppType() == apps::mojom::AppType::kWeb;
  if (!is_web_app) {
    info.set_app_id(update.AppId());
    info.set_app_name(update.Name());
  } else {
    const std::string launch_url = provider_->web_app_provider.registrar()
                                       .GetAppStartUrl(update.AppId())
                                       .GetOrigin()
                                       .spec();
    info.set_app_id(launch_url);
    info.set_app_name(launch_url);
  }
  info.set_status(ExtractStatus(update.Readiness()));
  info.set_version(update.Version());
  info.set_app_type(ExtractAppType(update.AppType()));

  *info.mutable_active_time_periods() = {app_activity.begin(),
                                         app_activity.end()};
  return info;
}

void AppInfoGenerator::SetOpenDurationsToClosed(base::Time end_time) {
  if (!provider_) {
    return;
  }
  provider_->app_service_proxy.InstanceRegistry().RemoveObserver(this);
  for (auto const& app : app_instances_by_id_) {
    const std::string& app_id = app.first;
    base::Time start_time = app.second.get()->start_time;
    provider_->activity_storage.AddActivityPeriod(start_time, end_time, app_id);
  }
  app_instances_by_id_.clear();
}

void AppInfoGenerator::SetIdleDurationsToOpen() {
  if (!provider_) {
    return;
  }
  base::Time start_time = clock_.Now();
  provider_->app_service_proxy.InstanceRegistry().ForEachInstance(
      [this, start_time](const apps::InstanceUpdate& update) {
        if (update.State() & apps::InstanceState::kStarted) {
          OpenUsageInterval(update.AppId(), update.Window(), start_time);
        }
      });
  provider_->app_service_proxy.InstanceRegistry().AddObserver(this);
}

void AppInfoGenerator::OpenUsageInterval(const std::string& app_id,
                                         aura::Window* window,
                                         const base::Time start_time) {
  if (app_instances_by_id_.count(app_id) == 0) {
    app_instances_by_id_[app_id] = std::make_unique<AppInstances>(start_time);
  }
  app_instances_by_id_[app_id]->running_instances.insert(window);
}

void AppInfoGenerator::CloseUsageInterval(const std::string& app_id,
                                          aura::Window* window,
                                          const base::Time end_time) {
  if (app_instances_by_id_.count(app_id)) {
    auto& app_instances = app_instances_by_id_[app_id];
    app_instances->running_instances.erase(window);
    if (app_instances->running_instances.empty()) {
      base::Time start_time = app_instances->start_time;
      provider_->activity_storage.AddActivityPeriod(start_time, end_time,
                                                    app_id);
      app_instances_by_id_.erase(app_id);
    }
  }
}

void AppInfoGenerator::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (!update.StateChanged()) {
    return;
  }
  apps::InstanceState state = update.State();
  const std::string& app_id = update.AppId();
  aura::Window* window = update.Window();
  if (state & apps::InstanceState::kStarted) {
    OpenUsageInterval(app_id, window, update.LastUpdatedTime());
  } else if (state & apps::InstanceState::kDestroyed) {
    CloseUsageInterval(app_id, window, update.LastUpdatedTime());
  }
}

void AppInfoGenerator::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* registry) {
  registry->RemoveObserver(this);
}

}  // namespace policy
