// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ANIMATION_METRICS_TEST_UTIL_H_
#define ASH_TEST_ANIMATION_METRICS_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_observer.h"

namespace ash::test {

class TestAnimationObserver final : public ui::CompositorAnimationObserver {
 public:
  explicit TestAnimationObserver(ui::Compositor* compositor);
  TestAnimationObserver(const TestAnimationObserver&) = delete;
  TestAnimationObserver& operator=(const TestAnimationObserver&) = delete;
  ~TestAnimationObserver() override;

  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

 private:
  int count_ = 0;
  const raw_ptr<ui::Compositor> compositor_;
};

class FirstNonAnimatedFrameStartedWaiter : public ui::CompositorObserver {
 public:
  explicit FirstNonAnimatedFrameStartedWaiter(ui::Compositor* compositor);
  ~FirstNonAnimatedFrameStartedWaiter() override;

  // ui::CompositorObserver
  void OnFirstNonAnimatedFrameStarted(ui::Compositor* compositor) override;

  void Wait();

 private:
  raw_ptr<ui::Compositor> compositor_;
  bool done_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class MetricsWaiter {
 public:
  MetricsWaiter(base::HistogramTester* histogram_tester,
                std::string metrics_name);
  MetricsWaiter(const MetricsWaiter&) = delete;
  MetricsWaiter& operator=(const MetricsWaiter&) = delete;
  ~MetricsWaiter();

  void Wait();

 private:
  raw_ptr<base::HistogramTester> histogram_tester_;
  const std::string metrics_name_;
};

void RunSimpleAnimation();

}  // namespace ash::test

#endif  // ASH_TEST_ANIMATION_METRICS_TEST_UTIL_H_
