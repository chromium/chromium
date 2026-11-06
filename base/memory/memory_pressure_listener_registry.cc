// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener_registry.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_level.h"
#include "base/memory_coordinator/memory_pressure_source.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"

namespace base {

namespace {

MemoryPressureListenerRegistry* g_memory_pressure_listener_registry = nullptr;

}  // namespace

// static
bool MemoryPressureListenerRegistry::Exists() {
  return g_memory_pressure_listener_registry;
}

// static
MemoryPressureListenerRegistry& MemoryPressureListenerRegistry::Get() {
  CHECK(g_memory_pressure_listener_registry);
  return *g_memory_pressure_listener_registry;
}

// static
MemoryPressureListenerRegistry* MemoryPressureListenerRegistry::MaybeGet() {
  return g_memory_pressure_listener_registry;
}

MemoryPressureListenerRegistry::MemoryPressureListenerRegistry()
    : memory_pressure_subscription_(MemoryPressureSource::Get().AddCallback(
          BindRepeating(&MemoryPressureListenerRegistry::OnMemoryPressure,
                        Unretained(this)))) {
  // The registry must observe every notification sent by the source, so that
  // `last_memory_pressure_level_` is accurate.
  CHECK_EQ(MemoryPressureSource::Get().GetLastNotifiedMemoryPressureLevel(
               PassKey<MemoryPressureListenerRegistry>()),
           MEMORY_PRESSURE_LEVEL_NONE);
  CHECK(!g_memory_pressure_listener_registry);
  g_memory_pressure_listener_registry = this;
}

MemoryPressureListenerRegistry::~MemoryPressureListenerRegistry() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  listeners_.Notify(&MemoryPressureListenerRegistration::
                        OnBeforeMemoryPressureListenerRegistryDestroyed);
  CHECK(listeners_.empty());

  CHECK_EQ(g_memory_pressure_listener_registry, this);
  g_memory_pressure_listener_registry = nullptr;
}

// static
void MemoryPressureListenerRegistry::NotifyMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());
  if (auto* source = MemoryPressureSource::MaybeGet()) {
    source->SetMemoryPressureLevel(memory_pressure_level);
  }
}

// static
void MemoryPressureListenerRegistry::NotifyMemoryPressureFromAnyThread(
    MemoryPressureLevel memory_pressure_level) {
  MemoryPressureSource::SetMemoryPressureLevelFromAnyThread(
      memory_pressure_level);
}

void MemoryPressureListenerRegistry::AddObserver(
    MemoryPressureListenerRegistration* listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());
  listeners_.AddObserver(listener);

  if (last_memory_pressure_level_ == MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  listener->SetInitialMemoryPressureLevel(
      PassKey<MemoryPressureListenerRegistry>(), last_memory_pressure_level_);
}

void MemoryPressureListenerRegistry::RemoveObserver(
    MemoryPressureListenerRegistration* listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  listeners_.RemoveObserver(listener);
}

// static
bool MemoryPressureListenerRegistry::AreNotificationsSuppressed() {
  return MemoryPressureSource::AreNotificationsSuppressed();
}

// static
void MemoryPressureListenerRegistry::SimulatePressureNotification(
    MemoryPressureLevel memory_pressure_level) {
  MemoryPressureSource::SimulatePressureNotification(memory_pressure_level);
}

void MemoryPressureListenerRegistry::OnMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  last_memory_pressure_level_ = memory_pressure_level;
  listeners_.Notify(
      &MemoryPressureListenerRegistration::UpdateMemoryPressureLevel,
      PassKey<MemoryPressureListenerRegistry>(), memory_pressure_level);
}

}  // namespace base
