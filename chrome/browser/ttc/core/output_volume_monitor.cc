// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/output_volume_monitor.h"

#include <utility>

#include "base/no_destructor.h"

namespace ttc {

namespace {

OutputVolumeMonitor::Factory& GetFactoryForTesting() {
  static base::NoDestructor<OutputVolumeMonitor::Factory> factory;
  return *factory;
}

}  // namespace

OutputVolumeMonitor::ScopedFactoryForTesting::ScopedFactoryForTesting(
    Factory factory)
    : reset_(&GetFactoryForTesting(), std::move(factory)) {}

OutputVolumeMonitor::ScopedFactoryForTesting::~ScopedFactoryForTesting() =
    default;

// static
std::unique_ptr<OutputVolumeMonitor> OutputVolumeMonitor::Create(
    StateChangedCallback callback) {
  if (const Factory& factory = GetFactoryForTesting()) {
    return factory.Run(std::move(callback));
  }
  return CreateForPlatform(std::move(callback));
}

// TODO(migueloruiz): Read the output volume on each platform.
// static
std::unique_ptr<OutputVolumeMonitor> OutputVolumeMonitor::CreateForPlatform(
    StateChangedCallback callback) {
  return nullptr;
}

}  // namespace ttc
