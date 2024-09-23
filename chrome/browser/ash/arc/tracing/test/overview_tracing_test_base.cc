// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/test/overview_tracing_test_base.h"

#include "ash/constants/ash_switches.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/ash/arc/tracing/test/overview_tracing_test_handler.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "content/public/test/browser_task_environment.h"

namespace arc {

OverviewTracingTestBase::OverviewTracingTestBase()
    : ash::AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
          std::make_unique<content::BrowserTaskEnvironment>(
              base::test::TaskEnvironment::TimeSource::MOCK_TIME))) {}

OverviewTracingTestBase::~OverviewTracingTestBase() = default;

void OverviewTracingTestBase::SetUp() {
  ash::AshTestBase::SetUp();

  profile_ = std::make_unique<TestingProfile>();
  arc_app_test_.SetUp(profile_.get());

  // WMHelper constructor sets a global instance which the Handler constructor
  // requires.
  wm_helper_ = std::make_unique<exo::WMHelper>();
  download_path_ = base::GetTempDirForTesting();

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVm);

  saved_tz_.reset(icu::TimeZone::createDefault());
}

// static
void OverviewTracingTestBase::SetTimeZone(const char* name) {
  std::unique_ptr<icu::TimeZone> tz{icu::TimeZone::createTimeZone(name)};
  icu::TimeZone::setDefault(*tz);
}

void OverviewTracingTestBase::TearDown() {
  icu::TimeZone::setDefault(*saved_tz_);

  wm_helper_.reset();

  arc_app_test_.TearDown();

  profile_.reset();

  ash::AshTestBase::TearDown();
}

void OverviewTracingTestBase::CommitAndPresentFrames(
    arc::OverviewTracingTestHandler* handler,
    exo::Surface* surface,
    int count,
    base::TimeDelta delta) {
  for (int i = 0; i < count; i++) {
    surface->Commit();
    std::list<exo::Surface::FrameCallback> frame_callbacks;
    std::list<exo::Surface::PresentationCallback> presentation_callbacks;
    surface->AppendSurfaceHierarchyCallbacks(&frame_callbacks,
                                             &presentation_callbacks);
    gfx::PresentationFeedback feedback(handler->SystemTicksNow(),
                                       /*interval=*/base::TimeDelta(),
                                       /*flags=*/0);
    for (auto& cb : presentation_callbacks) {
      cb.Run(feedback);
    }
    FastForwardClockAndTaskQueue(handler, delta);
  }
}

void OverviewTracingTestBase::FastForwardClockAndTaskQueue(
    arc::OverviewTracingTestHandler* handler,
    base::TimeDelta delta) {
  handler->set_now(handler->Now() + delta);
  task_environment()->FastForwardBy(delta);
}

}  // namespace arc
