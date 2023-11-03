// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/gfx/geometry/rect.h"

class ArcAppTest;

namespace aura {
class Window;
}  // namespace aura

namespace base::test {
class TaskEnvironment;
}  // namespace base::test

namespace views {
class Widget;
}  // namespace views

namespace arc::input_overlay {

// I/O time to wait.
constexpr base::TimeDelta kIORead = base::Milliseconds(50);

inline constexpr char kEnabledPackageName[] =
    "org.chromium.arc.testapp.inputoverlay";

class TouchInjector;

// Create ARC window without exo support.
std::unique_ptr<views::Widget> CreateArcWindow(
    aura::Window* root_window,
    const gfx::Rect& bounds = gfx::Rect(10, 10, 100, 100),
    const std::string& package_name = std::string("arc.packagename"));

// Make sure the tasks run synchronously when creating the window.
std::unique_ptr<views::Widget> CreateArcWindowSyncAndWait(
    base::test::TaskEnvironment* task_environment,
    aura::Window* root_window,
    const gfx::Rect& bounds,
    const std::string& package_name);

// Check the actions size in `injector`, action types, and action IDs.
void CheckActions(TouchInjector* injector,
                  size_t expect_size,
                  const std::vector<ActionType>& expect_types,
                  const std::vector<int>& expect_ids);

void SimulatedAppInstalled(base::test::TaskEnvironment* task_environment,
                           ArcAppTest& arc_app_test,
                           const std::string& package_name,
                           bool is_gc_opt_out,
                           bool is_game);

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_TEST_UTILS_H_
