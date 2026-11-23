// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/output_volume_monitor.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

class FakeOutputVolumeMonitor : public OutputVolumeMonitor {
 public:
  std::optional<OutputVolumeState> GetState() const override {
    return OutputVolumeState{.muted = true, .level = 0.5f};
  }
};

std::unique_ptr<OutputVolumeMonitor> CreateFake(
    OutputVolumeMonitor::StateChangedCallback callback) {
  return std::make_unique<FakeOutputVolumeMonitor>();
}

TEST(OutputVolumeMonitorTest, FactoryForTestingReplacesPlatformMonitor) {
  {
    OutputVolumeMonitor::ScopedFactoryForTesting factory(
        base::BindRepeating(&CreateFake));
    std::unique_ptr<OutputVolumeMonitor> monitor =
        OutputVolumeMonitor::Create(base::DoNothing());
    ASSERT_TRUE(monitor);
    EXPECT_EQ(monitor->GetState(),
              (OutputVolumeState{.muted = true, .level = 0.5f}));
  }
  // Without the factory, `Create()` falls back to the platform's monitor,
  // which unit tests don't have.
  EXPECT_FALSE(OutputVolumeMonitor::Create(base::DoNothing()));
}

}  // namespace

}  // namespace ttc
