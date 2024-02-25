// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TIMER_ARC_TIMER_BRIDGE_H_
#define ASH_COMPONENTS_ARC_TIMER_ARC_TIMER_BRIDGE_H_

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/timer.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

constexpr char kArcSetTimeJobName[] = "arc_2dset_2dtime";

// TimerHost::SetTime rejects the request if delta between requested time and
// current time is greater than this value.
constexpr base::TimeDelta kArcSetTimeMaxTimeDelta = base::Hours(24);

class ArcBridgeService;

// Sets wake up timers / alarms based on calls from the instance.
class ArcTimerBridge : public KeyedService,
                       public ConnectionObserver<mojom::TimerInstance>,
                       public mojom::TimerHost {
 public:
  using TimerId = int32_t;

  // Returns the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcTimerBridge* GetForBrowserContext(content::BrowserContext* context);

  static ArcTimerBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcTimerBridge(content::BrowserContext* context,
                 ArcBridgeService* bridge_service);

  ArcTimerBridge(const ArcTimerBridge&) = delete;
  ArcTimerBridge& operator=(const ArcTimerBridge&) = delete;

  ~ArcTimerBridge() override;

  // ConnectionObserver<mojom::TimerInstance>::Observer overrides.
  void OnConnectionClosed() override;

  // mojom::TimerHost overrides.
  void CreateTimers(
      std::vector<arc::mojom::CreateTimerRequestPtr> arc_timer_requests,
      CreateTimersCallback callback) override;
  void StartTimer(clockid_t clock_id,
                  base::TimeTicks absolute_expiration_time,
                  StartTimerCallback callback) override;
  void SetTime(base::Time time, SetTimeCallback callback) override;

  static void EnsureFactoryBuilt();

 private:
  // Deletes all timers.
  void DeleteArcTimers();

  // Callback for (powerd API) call made in |DeleteArcTimers|.
  void OnDeleteArcTimers(bool result);

  // Callback for powerd's D-Bus API called in |CreateTimers|.
  void OnCreateArcTimers(std::vector<clockid_t> clock_ids,
                         CreateTimersCallback callback,
                         std::optional<std::vector<TimerId>> timer_ids);

  // Retrieves the timer id corresponding to |clock_id|. If a mapping exists in
  // |timer_ids_| then returns an int32_t >= 0. Else returns std::nullopt.
  std::optional<TimerId> GetTimerId(clockid_t clock_id) const;

  // Owned by ArcServiceManager.
  const raw_ptr<ArcBridgeService> arc_bridge_service_;

  // Mapping of clock ids (coresponding to <sys/timerfd.h>) sent by the instance
  // in |CreateTimers| to timer ids returned in |OnCreateArcTimersDBusMethod|.
  std::map<clockid_t, TimerId> timer_ids_;

  mojo::Receiver<mojom::TimerHost> receiver_{this};

  base::WeakPtrFactory<ArcTimerBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TIMER_ARC_TIMER_BRIDGE_H_
