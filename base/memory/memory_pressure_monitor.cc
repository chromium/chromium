// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_monitor.h"

#include "base/check.h"
#include "base/metrics/histogram.h"
#include "base/notreached.h"

namespace base {
namespace {

MemoryPressureMonitor* g_monitor = nullptr;

}  // namespace

MemoryPressureMonitor::MemoryPressureMonitor() {
  DCHECK(!g_monitor);
  g_monitor = this;
}

MemoryPressureMonitor::~MemoryPressureMonitor() {
  DCHECK(g_monitor);
  g_monitor = nullptr;
}

// static
MemoryPressureMonitor* MemoryPressureMonitor::Get() {
  return g_monitor;
}

}  // namespace base
