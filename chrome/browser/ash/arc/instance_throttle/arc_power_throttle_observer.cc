// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_power_throttle_observer.h"
#include "base/time/time.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

// Time to recover ANR by default.
constexpr base::TimeDelta kHandleDefaultAnrTime = base::Seconds(10);
// Time to recover ANR in services.
constexpr base::TimeDelta kHandleServiceAnrTime = base::Seconds(20);
// Time to recover ANR in broadcast queue.
// TODO(khmel): This might require to separate foreground/background queue.
constexpr base::TimeDelta kHandleBroadcastAnrTime = base::Seconds(10);

}  // namespace

ArcPowerThrottleObserver::ArcPowerThrottleObserver()
    : ThrottleObserver(kArcPowerThrottleObserverName) {}

ArcPowerThrottleObserver::~ArcPowerThrottleObserver() = default;

void ArcPowerThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);
  auto* const power_bridge = ArcPowerBridge::GetForBrowserContext(context);
  // Could be nullptr in unit tests.
  if (power_bridge)
    powerbridge_observation_.Observe(power_bridge);
}

void ArcPowerThrottleObserver::StopObserving() {
  // Make sure |timer_| is not fired after stopping observing.
  timer_.Stop();

  powerbridge_observation_.Reset();

  ThrottleObserver::StopObserving();
}

void ArcPowerThrottleObserver::OnPreAnr(mojom::AnrType type) {
  VLOG(1) << "Handle pre-ANR state in " << type;
  // Android system server detects the situation that ANR crash may happen
  // soon. This might be caused by ARC throttling when Android does not have
  // enough CPU power to flash pending requests. Disable throttling in this
  // case for kHandleAnrTime| period in order to let the system server to flash
  // requests.
  SetActive(true);

  base::TimeDelta delta;
  switch (type) {
    case mojom::AnrType::FOREGROUND_SERVICE:
    case mojom::AnrType::BACKGROUND_SERVICE:
      delta = kHandleServiceAnrTime;
      break;
    case mojom::AnrType::BROADCAST:
      delta = kHandleBroadcastAnrTime;
      break;
    default:
      delta = kHandleDefaultAnrTime;
  }

  // Automatically inactivate this lock in |delta| time. Note, if we
  // would receive another pre-ANR event, timer might be re-activated and
  // this lock might be extended.
  // base::Unretained(this) is safe here due to |timer_| is owned by this
  // class and timer is autoatically canceled on DTOR.
  const base::TimeTicks new_expected = base::TimeTicks::Now() + delta;
  if (timer_.desired_run_time() < new_expected) {
    timer_.Start(FROM_HERE, delta,
                 base::BindOnce(&ArcPowerThrottleObserver::SetActive,
                                base::Unretained(this), false));
  }
}

void ArcPowerThrottleObserver::OnWillDestroyArcPowerBridge() {
  // No more notifications about VM resumed.
  powerbridge_observation_.Reset();
}

}  // namespace arc
