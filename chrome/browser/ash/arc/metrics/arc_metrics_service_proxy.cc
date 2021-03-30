// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/metrics/arc_metrics_service_proxy.h"

#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/memory/memory_kills_monitor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"

namespace arc {
namespace {

class ArcMetricsServiceProxyFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMetricsServiceProxy,
          ArcMetricsServiceProxyFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMetricsServiceProxyFactory";

  static ArcMetricsServiceProxyFactory* GetInstance() {
    return base::Singleton<ArcMetricsServiceProxyFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcMetricsServiceProxyFactory>;

  ArcMetricsServiceProxyFactory() {
    DependsOn(ArcAppListPrefsFactory::GetInstance());
    DependsOn(ArcMetricsService::GetFactory());
  }

  ~ArcMetricsServiceProxyFactory() override = default;
};

}  // namespace

// static
ArcMetricsServiceProxy* ArcMetricsServiceProxy::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMetricsServiceProxyFactory::GetForBrowserContext(context);
}

ArcMetricsServiceProxy::ArcMetricsServiceProxy(
    content::BrowserContext* context,
    ArcBridgeService* arc_bridge_service)
    : arc_app_list_prefs_(ArcAppListPrefs::Get(context)),
      arc_metrics_service_(ArcMetricsService::GetForBrowserContext(context)) {
  arc_app_list_prefs_->AddObserver(this);
  arc::ArcSessionManager::Get()->AddObserver(this);
  arc_metrics_service_->AddAppKillObserver(this);
}

void ArcMetricsServiceProxy::Shutdown() {
  arc::ArcSessionManager::Get()->RemoveObserver(this);
  arc_app_list_prefs_->RemoveObserver(this);
  arc_metrics_service_->RemoveAppKillObserver(this);
}

void ArcMetricsServiceProxy::OnTaskCreated(int32_t task_id,
                                           const std::string& package_name,
                                           const std::string& activity,
                                           const std::string& intent,
                                           int32_t session_id) {
  arc_metrics_service_->OnTaskCreated(task_id, package_name, activity, intent);
}

void ArcMetricsServiceProxy::OnTaskDestroyed(int32_t task_id) {
  arc_metrics_service_->OnTaskDestroyed(task_id);
}

void ArcMetricsServiceProxy::OnArcSessionStopped(ArcStopReason stop_reason) {
  const auto* profile = ProfileManager::GetPrimaryUserProfile();
  if (arc::IsArcAllowedForProfile(profile)) {
    base::UmaHistogramEnumeration(
        GetHistogramNameByUserType("Arc.Session.StopReason", profile),
        stop_reason);
  }
}

void ArcMetricsServiceProxy::OnArcLowMemoryKill() {
  memory::MemoryKillsMonitor::LogLowMemoryKill("APP", 0);
}

void ArcMetricsServiceProxy::OnArcOOMKillCount(
    unsigned long current_oom_kills) {
  memory::MemoryKillsMonitor::LogArcOOMKill(current_oom_kills);
}

}  // namespace arc
