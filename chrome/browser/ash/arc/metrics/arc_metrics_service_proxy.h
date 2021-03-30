// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_METRICS_ARC_METRICS_SERVICE_PROXY_H_
#define CHROME_BROWSER_ASH_ARC_METRICS_ARC_METRICS_SERVICE_PROXY_H_

#include "base/macros.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/metrics/arc_metrics_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Proxy to ArcMetricsService for functionalities that depend on code under
// chrome/browser. Should be merged into ArcMetricsService once dependency
// issues are cleared. TODO(crbug.com/903048): Remove the proxy.
class ArcMetricsServiceProxy : public KeyedService,
                               public ArcAppListPrefs::Observer,
                               public ArcSessionManagerObserver,
                               public ArcMetricsService::AppKillObserver {
 public:
  static ArcMetricsServiceProxy* GetForBrowserContext(
      content::BrowserContext* context);

  ArcMetricsServiceProxy(content::BrowserContext* context,
                         ArcBridgeService* arc_bridge_service);
  ~ArcMetricsServiceProxy() override = default;

  // KeyedService overrides.
  void Shutdown() override;

  // ArcAppListPrefs::Observer overrides.
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;
  void OnTaskDestroyed(int32_t task_id) override;

  // ArcSessionManagerObserver overrides.
  void OnArcSessionStopped(ArcStopReason stop_reason) override;

  // ArcMetricsService::AppKillObserver overrides.
  void OnArcLowMemoryKill() override;
  void OnArcOOMKillCount(unsigned long current_oom_kills) override;

 private:
  ArcAppListPrefs* const arc_app_list_prefs_;
  ArcMetricsService* const arc_metrics_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcMetricsServiceProxy);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_METRICS_ARC_METRICS_SERVICE_PROXY_H_
