// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory_coordinator/utils.h"
#include "base/notreached.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/tracing_buildflags.h"

namespace base {

namespace {

MemoryLimit GetMemoryLimitForMemoryPressureLevel(MemoryPressureLevel level) {
  switch (level) {
    case MEMORY_PRESSURE_LEVEL_NONE:
      return kNoMemoryPressureThreshold;
    case MEMORY_PRESSURE_LEVEL_MODERATE:
      return kModerateMemoryPressureThreshold;
    case MEMORY_PRESSURE_LEVEL_CRITICAL:
      return kCriticalMemoryPressureThreshold;
  }
  NOTREACHED();
}

}  // namespace

// MemoryPressureListener ------------------------------------------------------

MemoryPressureListener::MemoryPressureListener() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
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
void MemoryPressureListener::SimulatePressureNotification(
    MemoryPressureLevel memory_pressure_level) {
  MemoryPressureListenerRegistry::SimulatePressureNotification(
      memory_pressure_level);
}

MemoryLimit MemoryPressureListener::GetMemoryLimit() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetMemoryLimitForMemoryPressureLevel(memory_pressure_level_);
}

void MemoryPressureListener::SetInitialMemoryPressureLevel(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  memory_pressure_level_ = memory_pressure_level;
}

void MemoryPressureListener::UpdateMemoryPressureLevel(
    MemoryPressureLevel memory_pressure_level,
    bool ignore_repeated_notifications) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (memory_pressure_level_ == memory_pressure_level &&
      ignore_repeated_notifications) {
    return;
  }

  memory_pressure_level_ = memory_pressure_level;
  OnMemoryPressure(memory_pressure_level);
}

// MemoryPressureListenerRegistration --------------------------------------

MemoryPressureListenerRegistration::MemoryPressureListenerRegistration(
    MemoryPressureListenerTag tag,
    MemoryPressureListener* memory_pressure_listener,
    bool ignore_repeated_notifications)
    : tag_(tag),
      memory_pressure_listener_(memory_pressure_listener),
      ignore_repeated_notifications_(ignore_repeated_notifications),
      registry_(MemoryPressureListenerRegistry::MaybeGet()) {
  if (!registry_) {
    DVLOG(1) << "Registration of a MemoryPressureListener failed. The "
                "MemoryPressureListenerRegistry doesn't exist.";
    return;
  }

  registry_->AddObserver(this);
}

MemoryPressureListenerRegistration::MemoryPressureListenerRegistration(
    const Location& creation_location,
    MemoryPressureListenerTag tag,
    MemoryPressureListener* memory_pressure_listener,
    bool ignore_repeated_notifications)
    : MemoryPressureListenerRegistration(tag,
                                         memory_pressure_listener,
                                         ignore_repeated_notifications) {}

MemoryPressureListenerRegistration::~MemoryPressureListenerRegistration() {
  if (!registry_) {
    return;
  }

  CHECK_EQ(registry_, MemoryPressureListenerRegistry::MaybeGet());
  registry_->RemoveObserver(this);
}

void MemoryPressureListenerRegistration::
    OnBeforeMemoryPressureListenerRegistryDestroyed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  registry_->RemoveObserver(this);
  registry_ = nullptr;
}

void MemoryPressureListenerRegistration::SetInitialMemoryPressureLevel(
    PassKey<MemoryPressureListenerRegistry>,
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  memory_pressure_listener_->SetInitialMemoryPressureLevel(
      memory_pressure_level);
}

void MemoryPressureListenerRegistration::UpdateMemoryPressureLevel(
    PassKey<MemoryPressureListenerRegistry>,
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  memory_pressure_listener_->UpdateMemoryPressureLevel(
      memory_pressure_level, ignore_repeated_notifications_);
}

}  // namespace base
