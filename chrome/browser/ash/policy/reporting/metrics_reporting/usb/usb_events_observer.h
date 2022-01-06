// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_USB_USB_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_USB_USB_EVENTS_OBSERVER_H_

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_events_observer_base.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

using UsbEventInfoPtr = chromeos::cros_healthd::mojom::UsbEventInfoPtr;

namespace reporting {

class UsbEventsObserver
    : public reporting::CrosHealthdEventsObserverBase<
          chromeos::cros_healthd::mojom::CrosHealthdUsbObserver>,
      public chromeos::cros_healthd::mojom::CrosHealthdUsbObserver {
 public:
  UsbEventsObserver();

  UsbEventsObserver(const UsbEventsObserver& other) = delete;
  UsbEventsObserver& operator=(const UsbEventsObserver& other) = delete;

  ~UsbEventsObserver() override;

  // chromeos::cros_healthd::mojom::CrosHealthdUsbObserver:
  void OnAdd(UsbEventInfoPtr info) override;

  void OnRemove(UsbEventInfoPtr info) override;

 protected:
  // CrosHealthdEventsObserverBase
  void AddObserver() override;

 private:
  void FillUsbTelemetry(UsbTelemetry* data, UsbEventInfoPtr info);
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_USB_USB_EVENTS_OBSERVER_H_
