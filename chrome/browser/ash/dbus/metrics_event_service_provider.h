// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_METRICS_EVENT_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_METRICS_EVENT_SERVICE_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chromeos/ash/components/dbus/metrics_event/metrics_event.pb.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace ash {

// This class does not export any methods.  An instance of this class can send
// signals to clients for a number of events of statistical interest, e.g. tab
// discards.
class MetricsEventServiceProvider
    : public CrosDBusService::ServiceProviderInterface,
      public resource_coordinator::TabLifecycleObserver {
 public:
  MetricsEventServiceProvider();

  MetricsEventServiceProvider(const MetricsEventServiceProvider&) = delete;
  MetricsEventServiceProvider& operator=(const MetricsEventServiceProvider&) =
      delete;

  ~MetricsEventServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // resource_coordinator::TabLifecycleObserver:
  void OnDiscardedStateChange(content::WebContents* contents,
                              LifecycleUnitDiscardReason reason,
                              bool is_discarded) override;

  // Emits the D-Bus signal for this event.
  void EmitSignal(metrics_event::Event_Type type);

  // A reference on ExportedObject for sending signals.
  scoped_refptr<dbus::ExportedObject> exported_object_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_METRICS_EVENT_SERVICE_PROVIDER_H_
