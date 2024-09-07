// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/throttle/throttle_observer.h"
#include "chromeos/ash/components/throttle/throttle_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class TimeDelta;
}

namespace content {
class BrowserContext;
}

namespace arc {
class ArcBootPhaseThrottleObserver;
class ArcBridgeService;

namespace mojom {
class PowerInstance;
}

// This class holds a number of observers which watch for several conditions
// (window activation, mojom instance connection, etc) and adjusts the
// throttling state of the ARC container on a change in conditions.
class ArcInstanceThrottle : public KeyedService,
                            public ash::ThrottleService,
                            public ConnectionObserver<mojom::PowerInstance>,
                            public ArcMetricsService::BootTypeObserver {
 public:
  // The name of the observer which monitors chrome://arc-power-control.
  static const char kChromeArcPowerControlPageObserver[];

  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual void SetCpuRestriction(CpuRestrictionState cpu_restriction_state,
                                   bool use_quota) = 0;
    virtual void RecordCpuRestrictionDisabledUMA(
        const std::string& observer_name,
        base::TimeDelta delta) = 0;
  };

  // Returns singleton instance for the given BrowserContext, or nullptr if
  // the browser |context| is not allowed to use ARC.
  static ArcInstanceThrottle* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcInstanceThrottle* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcInstanceThrottle(content::BrowserContext* context,
                      ArcBridgeService* bridge);
  ~ArcInstanceThrottle() override;

  ArcInstanceThrottle(const ArcInstanceThrottle&) = delete;
  ArcInstanceThrottle& operator=(const ArcInstanceThrottle&) = delete;

  // KeyedService:
  void Shutdown() override;

  // ConnectionObserver<mojom::PowerInstance>:
  void OnConnectionReady() override;

  void set_delegate_for_testing(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

  // ArcMetricsService::BootTypeObserver
  void OnBootTypeRetrieved(mojom::BootType boot_type) override;

  static void EnsureFactoryBuilt();

 private:
  // ash::ThrottleService:
  void ThrottleInstance(bool should_throttle) override;
  void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                       base::TimeDelta delta) override;

  // Notifies CPU resetriction state to power mojom.
  void NotifyCpuRestriction(CpuRestrictionState cpu_restriction_state);

  ArcBootPhaseThrottleObserver* GetBootObserver();

  std::unique_ptr<Delegate> delegate_;

  // True if CPU_RESTRICTION_BACKGROUND_WITH_CFS_QUOTA_ENFORCED should never be
  // used. By default, CPU quota enforcement is allowed (see the default value
  // below), but once one of the following conditions is met, the variable turns
  // `true` to completely disable the enforcement feature:
  //
  // * ARC is unthrottled by a user action (vs for faster boot or ANR
  //   prevention.)
  // * ARC's boot type is 'regular boot' (vs first boot or first boot after AU.)
  bool never_enforce_quota_ = false;

  // Owned by ArcServiceManager.
  const raw_ptr<ArcBridgeService> bridge_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_
