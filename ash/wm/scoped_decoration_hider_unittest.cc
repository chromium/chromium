// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/scoped_decoration_hider.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_properties.h"
#include "base/scoped_observation.h"
#include "chromeos/ui/base/app_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/decoration/decoration.h"
#include "ui/wm/core/decoration_controller.h"

namespace ash {
namespace {

class DestroyingHiderResetter : public aura::WindowObserver {
 public:
  explicit DestroyingHiderResetter(aura::Window* window) : hider_(window) {
    observation_.Observe(window);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    hider_.reset();
    observation_.Reset();
  }
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key == kDecorationHiderCountKey) {
      ADD_FAILURE();
    }
  }

 private:
  std::optional<ScopedDecorationHider> hider_;
  base::ScopedObservation<aura::Window, aura::WindowObserver> observation_{
      this};
};

}  // namespace

using ScopedDecorationHiderTest = AshTestBase;

// Tests that the decoration is hidden until all the hiders are destroyed.
TEST_F(ScopedDecorationHiderTest, HideDecoration) {
  auto window =
      CreateWindowWithAppType(chromeos::AppType::SYSTEM_APP, {200, 300});
  auto* decoration =
      Shell::Get()->decoration_controller()->GetDecorationForWindow(
          window.get());
  ASSERT_TRUE(decoration);
  EXPECT_TRUE(decoration->layer()->GetTargetVisibility());

  auto hider1 = std::make_unique<ScopedDecorationHider>(window.get());
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());

  auto hider2 = std::make_unique<ScopedDecorationHider>(window.get());
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());

  hider1.reset();
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());

  hider2.reset();
  EXPECT_TRUE(decoration->layer()->GetTargetVisibility());
}

// Tests move construction and move assignment of `ScopedDecorationHider`.
TEST_F(ScopedDecorationHiderTest, Move) {
  auto window1 =
      CreateWindowWithAppType(chromeos::AppType::SYSTEM_APP, {200, 300});
  auto window2 = CreateWindowWithAppType(chromeos::AppType::SYSTEM_APP,
                                         {250, 0, 200, 300});
  auto* decoration_controller = Shell::Get()->decoration_controller();
  auto* decoration1 =
      decoration_controller->GetDecorationForWindow(window1.get());
  auto* decoration2 =
      decoration_controller->GetDecorationForWindow(window2.get());
  ASSERT_TRUE(decoration1);
  ASSERT_TRUE(decoration2);

  {
    ScopedDecorationHider hider1(window1.get());
    EXPECT_FALSE(decoration1->layer()->GetTargetVisibility());

    ScopedDecorationHider moved_hider(std::move(hider1));
    EXPECT_FALSE(decoration1->layer()->GetTargetVisibility());

    // Move-assigning onto `moved_hider` unhides `window1` and keeps `window2`
    // hidden until `moved_hider` goes out of scope.
    moved_hider = ScopedDecorationHider(window2.get());
    EXPECT_TRUE(decoration1->layer()->GetTargetVisibility());
    EXPECT_FALSE(decoration2->layer()->GetTargetVisibility());
  }
  EXPECT_TRUE(decoration2->layer()->GetTargetVisibility());
}

// Tests that the hider can outlive the window or be destroyed during
// `OnWindowDestroying`.
TEST_F(ScopedDecorationHiderTest, WindowDestroyedFirst) {
  auto window1 =
      CreateWindowWithAppType(chromeos::AppType::SYSTEM_APP, {200, 300});
  auto hider = std::make_unique<ScopedDecorationHider>(window1.get());
  window1.reset();
  hider.reset();

  auto window2 =
      CreateWindowWithAppType(chromeos::AppType::SYSTEM_APP, {200, 300});
  DestroyingHiderResetter resetter(window2.get());
  window2.reset();
}

}  // namespace ash
