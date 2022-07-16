// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_power_throttle_observer.h"

namespace arc {

namespace {

constexpr base::TimeDelta kHandleAnrTime = base::Seconds(10);

}  // namespace

ArcPowerThrottleObserver::ArcPowerThrottleObserver()
    : ThrottleObserver(ThrottleObserver::PriorityLevel::CRITICAL, "ArcPower") {}

ArcPowerThrottleObserver::~ArcPowerThrottleObserver() = default;

void ArcPowerThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);
  auto* const power_bridge = ArcPowerBridge::GetForBrowserContext(context);
  // Could be nullptr in unit tests.
  if (power_bridge)
    power_bridge->AddObserver(this);
}

void ArcPowerThrottleObserver::StopObserving() {
  // Make sure |timer_| is not fired after stopping observing.
  timer_.Stop();

  auto* const power_bridge = ArcPowerBridge::GetForBrowserContext(context());
  // Could be nullptr in unit tests.
  if (power_bridge)
    power_bridge->RemoveObserver(this);

  ThrottleObserver::StopObserving();
}

void ArcPowerThrottleObserver::OnPreAnr(mojom::AnrType type) {
  VLOG(1) << "Handle pre-ANR state";
  // Android system server detects the situation that ANR crash may happen
  // soon. This might be caused by ARC throttling when Android does not have
  // enough CPU power to flash pending requests. Disable throttling in this
  // case for kHandleAnrTime| period in order to let the system server to flash
  // requests.
  SetActive(true);
  // Automatically inactivate this lock in |kHandleAnrTime|. Note, if we
  // would receive another pre-ANR event, timer would be re-activated and
  // this lock would be extended.
  // base::Unretained(this) is safe here due to |timer_| is owned by this
  // class and timer is automatically canceled on DTOR.
  timer_.Start(FROM_HERE, kHandleAnrTime,
               base::BindOnce(&ArcPowerThrottleObserver::SetActive,
                              base::Unretained(this), false));
}

}  // namespace arc
