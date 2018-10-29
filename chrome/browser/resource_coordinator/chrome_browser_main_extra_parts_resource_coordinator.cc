// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/chrome_browser_main_extra_parts_resource_coordinator.h"

#include "base/process/process.h"
#include "chrome/browser/resource_coordinator/browser_child_process_watcher.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"
#include "chrome/browser/resource_coordinator/render_process_probe.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/process_resource_coordinator.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

ChromeBrowserMainExtraPartsResourceCoordinator::
    ChromeBrowserMainExtraPartsResourceCoordinator() = default;
ChromeBrowserMainExtraPartsResourceCoordinator::
    ~ChromeBrowserMainExtraPartsResourceCoordinator() = default;

void ChromeBrowserMainExtraPartsResourceCoordinator::
    ServiceManagerConnectionStarted(
        content::ServiceManagerConnection* connection) {
  process_resource_coordinator_ =
      std::make_unique<resource_coordinator::ProcessResourceCoordinator>(
          connection->GetConnector());

  process_resource_coordinator_->SetLaunchTime(base::Time::Now());
  process_resource_coordinator_->SetPID(base::Process::Current().Pid());

  browser_child_process_watcher_ =
      std::make_unique<resource_coordinator::BrowserChildProcessWatcher>();
}

void ChromeBrowserMainExtraPartsResourceCoordinator::PreBrowserStart() {
  if (base::FeatureList::IsEnabled(features::kPerformanceMeasurement)) {
    DCHECK(resource_coordinator::RenderProcessProbe::IsEnabled());
    resource_coordinator::PageSignalReceiver* page_signal_receiver =
        resource_coordinator::PageSignalReceiver::GetInstance();

    DCHECK(resource_coordinator::PageSignalReceiver::IsEnabled());
    resource_coordinator::RenderProcessProbe* render_process_probe =
        resource_coordinator::RenderProcessProbe::GetInstance();

    performance_measurement_manager_ =
        std::make_unique<resource_coordinator::PerformanceMeasurementManager>(
            page_signal_receiver, render_process_probe);
  }
}
