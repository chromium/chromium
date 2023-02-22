// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_AUDIO_AUDIO_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_AUDIO_AUDIO_EVENTS_OBSERVER_H_

#include "chrome/browser/ash/policy/reporting/metrics_reporting/mojo_service_events_observer_base.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

namespace reporting {

class AudioEventsObserver : public MojoServiceEventsObserverBase<
                                ash::cros_healthd::mojom::EventObserver>,
                            public ash::cros_healthd::mojom::EventObserver {
 public:
  AudioEventsObserver();

  AudioEventsObserver(const AudioEventsObserver& other) = delete;
  AudioEventsObserver& operator=(const AudioEventsObserver& other) = delete;

  ~AudioEventsObserver() override;

  // ash::cros_healthd::mojom::EventObserver:
  void OnEvent(const ash::cros_healthd::mojom::EventInfoPtr info) override;

 protected:
  // CrosHealthdEventsObserverBase
  void AddObserver() override;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_AUDIO_AUDIO_EVENTS_OBSERVER_H_
