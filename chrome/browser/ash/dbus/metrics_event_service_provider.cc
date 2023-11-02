// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/metrics_event_service_provider.h"

#include <time.h>

#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

MetricsEventServiceProvider::MetricsEventServiceProvider() {}

MetricsEventServiceProvider::~MetricsEventServiceProvider() {
  DCHECK(g_browser_process);
  if (exported_object_)
    g_browser_process->GetTabManager()->RemoveObserver(this);
}

void MetricsEventServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object_ = exported_object;
  DCHECK(g_browser_process);
  g_browser_process->GetTabManager()->AddObserver(this);
}

void MetricsEventServiceProvider::OnDiscardedStateChange(
    content::WebContents* contents,
    LifecycleUnitDiscardReason reason,
    bool is_discarded) {
  if (is_discarded) {
    EmitSignal(metrics_event::Event_Type_TAB_DISCARD);
  }
}

void MetricsEventServiceProvider::EmitSignal(metrics_event::Event_Type type) {
  DCHECK(exported_object_);

  dbus::Signal signal(chromeos::kMetricsEventServiceInterface,
                      chromeos::kMetricsEventServiceChromeEventSignal);
  metrics_event::Event payload;

  // Here we have a Chrome/Chrome OS specific agreement to use clock_gettime
  // with CLOCK_MONOTONIC.
  struct timespec timespec;
  // clock_gettime() can only fail on bad pointer, or bad clock specifier, so
  // the following call "cannot" fail.
  int ret = clock_gettime(CLOCK_MONOTONIC, &timespec);
  if (ret != 0) {
    PLOG(DFATAL) << "clock_gettime";
    return;
  }
  int64_t now_ms = base::TimeDelta::FromTimeSpec(timespec).InMilliseconds();

  dbus::MessageWriter writer(&signal);
  payload.set_type(type);
  payload.set_timestamp(now_ms);
  dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(payload);
  exported_object_->SendSignal(&signal);
}

}  // namespace ash
