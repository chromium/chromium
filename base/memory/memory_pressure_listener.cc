// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include "base/memory/memory_pressure_listener_registry.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"

namespace base {

// MemoryPressureListener ------------------------------------------------------

MemoryPressureListener::MemoryPressureListener(
    const base::Location& creation_location,
    const MemoryPressureListener::MemoryPressureCallback& callback)
    : callback_(callback), creation_location_(creation_location) {
  MemoryPressureListenerRegistry::Get().AddObserver(this, false);
}

MemoryPressureListener::MemoryPressureListener(
    const base::Location& creation_location,
    const MemoryPressureListener::MemoryPressureCallback& callback,
    const MemoryPressureListener::SyncMemoryPressureCallback&
        sync_memory_pressure_callback)
    : callback_(callback),
      sync_memory_pressure_callback_(sync_memory_pressure_callback),
      creation_location_(creation_location) {
  MemoryPressureListenerRegistry::Get().AddObserver(this, true);
}

MemoryPressureListener::~MemoryPressureListener() {
  MemoryPressureListenerRegistry::Get().RemoveObserver(this);
}

void MemoryPressureListener::Notify(MemoryPressureLevel memory_pressure_level) {
  TRACE_EVENT(
      "base", "MemoryPressureListener::Notify",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_memory_pressure_notification();
        data->set_level(
            trace_event::MemoryPressureLevelToTraceEnum(memory_pressure_level));
        data->set_creation_location_iid(
            base::trace_event::InternedSourceLocation::Get(&ctx,
                                                           creation_location_));
      });
  callback_.Run(memory_pressure_level);
}

void MemoryPressureListener::SyncNotify(
    MemoryPressureLevel memory_pressure_level) {
  if (!sync_memory_pressure_callback_.is_null()) {
    sync_memory_pressure_callback_.Run(memory_pressure_level);
  }
}

// static
void MemoryPressureListener::NotifyMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  MemoryPressureListenerRegistry::NotifyMemoryPressure(memory_pressure_level);
}

// static
bool MemoryPressureListener::AreNotificationsSuppressed() {
  return MemoryPressureListenerRegistry::AreNotificationsSuppressed();
}

// static
void MemoryPressureListener::SetNotificationsSuppressed(bool suppressed) {
  MemoryPressureListenerRegistry::SetNotificationsSuppressed(suppressed);
}

// static
void MemoryPressureListener::SimulatePressureNotification(
    MemoryPressureLevel memory_pressure_level) {
  MemoryPressureListenerRegistry::SimulatePressureNotification(
      memory_pressure_level);
}

// SyncMemoryPressureListener --------------------------------------------------

SyncMemoryPressureListener::SyncMemoryPressureListener(
    SyncMemoryPressureCallback callback)
    : callback_(std::move(callback)),
      memory_pressure_listener_(
          FROM_HERE,
          DoNothing(),
          BindRepeating(&SyncMemoryPressureListener::OnMemoryPressure,
                        Unretained(this))) {}

SyncMemoryPressureListener::~SyncMemoryPressureListener() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SyncMemoryPressureListener::OnMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  callback_.Run(memory_pressure_level);
}

}  // namespace base
