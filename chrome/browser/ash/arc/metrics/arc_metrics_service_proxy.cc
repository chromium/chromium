// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/metrics/arc_metrics_service_proxy.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/memory/memory_kills_monitor.h"
#include "chrome/browser/memory/oom_kills_monitor.h"
#include "chrome/browser/profiles/profile_manager.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

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
  arc_metrics_service_->SetPrefService(g_browser_process->local_state());
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

void ArcMetricsServiceProxy::OnArcStarted() {
  arc_metrics_service_->OnArcStarted();
}

void ArcMetricsServiceProxy::OnArcSessionStopped(ArcStopReason stop_reason) {
  const auto* profile = ProfileManager::GetPrimaryUserProfile();
  if (arc::IsArcAllowedForProfile(profile)) {
    std::string metric_name =
        GetHistogramNameByUserType("Arc.Session.StopReason", profile);
    base::UmaHistogramEnumeration(metric_name, stop_reason);
    // Log the metric to facilitate finding feedback reports in Xamine.
    VLOG(1) << metric_name << ": "
            << static_cast<std::underlying_type_t<ArcStopReason>>(stop_reason);
  }
  arc_metrics_service_->OnArcSessionStopped();
}

void ArcMetricsServiceProxy::OnArcLowMemoryKill() {
  memory::MemoryKillsMonitor::LogLowMemoryKill("APP", 0);
}

void ArcMetricsServiceProxy::OnArcOOMKillCount(
    unsigned long current_oom_kills) {
  memory::OOMKillsMonitor::GetInstance().LogArcOOMKill(current_oom_kills);
}

void ArcMetricsServiceProxy::OnArcMemoryPressureKill(int count,
                                                     int estimated_freed_kb) {
  // TODO(b/197040216): update kill metrics to separate tab discards, LMKD
  // kills, and pressure kills.
  for (int i = 0; i < count; i++) {
    memory::MemoryKillsMonitor::LogLowMemoryKill("APP_PRESSURE", 0);
  }
}

// static
void ArcMetricsServiceProxy::EnsureFactoryBuilt() {
  ArcMetricsServiceProxyFactory::GetInstance();
}

}  // namespace arc
