// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/arc_resize_lock_manager.h"

#include <memory>
#include <string>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/compat_mode/metrics.h"
#include "ash/components/arc/compat_mode/test/compat_mode_test_base.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/resize_shadow_type.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/class_property.h"

namespace arc {
namespace {

class ScopedWindowPropertyObserver : public aura::WindowObserver {
 public:
  using WindowPropertyChangedCallback =
      base::RepeatingCallback<void(aura::Window*, const void*, intptr_t)>;

  ScopedWindowPropertyObserver(aura::Window* window,
                               WindowPropertyChangedCallback on_changed)
      : on_changed_(std::move(on_changed)) {
    observer_.Observe(window);
  }
  ~ScopedWindowPropertyObserver() override { observer_.Reset(); }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    on_changed_.Run(window, key, old);
  }
  void OnWindowDestroying(aura::Window* window) override { observer_.Reset(); }

 private:
  WindowPropertyChangedCallback on_changed_;
  base::ScopedObservation<aura::Window, aura::WindowObserver> observer_{this};
};

class TestCompatModeButtonController : public CompatModeButtonController {
 public:
  ~TestCompatModeButtonController() override = default;

  bool IsUpdateCompatModeButtonCalled(const aura::Window* window) const {
    return update_compat_mode_button_called.contains(window);
  }
  void ResetUpdateCompatModeButtonCalled() {
    update_compat_mode_button_called.clear();
  }

  // CompatModeButtonController:
  void Update(aura::Window* window) override {
    update_compat_mode_button_called.insert(window);
  }

 private:
  base::flat_set<raw_ptr<const aura::Window, CtnExperimental>>
      update_compat_mode_button_called;
};

class TestArcResizeLockManager : public ArcResizeLockManager {
 public:
  TestArcResizeLockManager() : ArcResizeLockManager(nullptr, nullptr) {}
  ~TestArcResizeLockManager() override = default;

  // ArcResizeLockManager:
  void ShowSplashScreenDialog(aura::Window* window, bool) override {
    show_splash_callback_.Run(window);
  }

  void set_show_splash_callback(
      base::RepeatingCallback<void(aura::Window*)> callback) {
    show_splash_callback_ = std::move(callback);
  }

 private:
  base::RepeatingCallback<void(aura::Window*)> show_splash_callback_;
};

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kNonInterestedPropKey, false)

constexpr std::array<ash::ArcResizeLockType, 4> kArcResizeLockTypes{
    ash::ArcResizeLockType::NONE,
    ash::ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE,
    ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE,
    ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE};

}  // namespace

class ArcResizeLockManagerTest : public CompatModeTestBase {
 public:
  // CompatModeTestBase:
  void SetUp() override {
    CompatModeTestBase::SetUp();
    arc_resize_lock_manager_.SetPrefDelegate(pref_delegate());

    auto controller = std::make_unique<TestCompatModeButtonController>();
    test_compat_mode_button_controller_ = controller.get();
    arc_resize_lock_manager_.compat_mode_button_controller_ =
        std::move(controller);
  }

  std::unique_ptr<aura::Window> CreateFakeWindow(bool is_arc) {
    auto window = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_NORMAL);
    if (is_arc) {
      window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
    }
    window->Init(ui::LAYER_TEXTURED);
    window->Show();
    return window;
  }

  bool IsResizeLockEnabled(const aura::Window* window) const {
    return arc_resize_lock_manager_.resize_lock_enabled_windows_.contains(
        window);
  }

  bool IsUpdateCompatModeButtonCalled(const aura::Window* window) const {
    return test_compat_mode_button_controller_->IsUpdateCompatModeButtonCalled(
        window);
  }
  void ResetUpdateCompatModeButtonCalled() {
    test_compat_mode_button_controller_->ResetUpdateCompatModeButtonCalled();
  }

  void SetShowSplashCallback(
      base::RepeatingCallback<void(aura::Window*)> callback) {
    arc_resize_lock_manager_.set_show_splash_callback(std::move(callback));
  }

 private:
  TestArcResizeLockManager arc_resize_lock_manager_;

  // Owned by |arc_resize_lock_manager_|.
  raw_ptr<TestCompatModeButtonController> test_compat_mode_button_controller_;
};

TEST_F(ArcResizeLockManagerTest, ConstructDestruct) {}

// Tests that resize lock state is properly sync'ed with the window property.
TEST_F(ArcResizeLockManagerTest, TestPropertyChange) {
  auto arc_window = CreateFakeWindow(true);
  std::string app_id = "app-id";

  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  // App id needs to be set to toogle resize lock state.
  arc_window->SetProperty(ash::kAppIDKey, std::string("app-id"));
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  // Test EnableResizeLock will be called by the property change.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window.get()));

  // Test nothing will be called by the property overwrite with the same value.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window.get()));

  // Test DisableResizeLock will be called by the property change.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::NONE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  // Test if enabling/disabling |RESIZE_DISABLED_NONTOGGLABLE| toggles the
  // resize lock state properly.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window.get()));
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::NONE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  // Test if resize lock state is updated even when resizability doesn't
  // change (NONE->RESIZE_ENABLED_TOGGLABLE).
  EXPECT_EQ(pref_delegate()->GetResizeLockState(app_id),
            mojom::ArcResizeLockState::FULLY_LOCKED);
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE);
  EXPECT_EQ(pref_delegate()->GetResizeLockState(app_id),
            mojom::ArcResizeLockState::OFF);

  // Test nothing will be called by the property overwrite with the same value.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::NONE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  // Test nothing will be called by the NON-interested property change.
  arc_window->SetProperty(kNonInterestedPropKey, true);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));
}

// Test resize lock will not be enabled right after property change but
// will be after the app id is set to the non-null value.
TEST_F(ArcResizeLockManagerTest, TestPropertyChangeWithDelayedAppId) {
  auto arc_window = CreateFakeWindow(true);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));
  // Should ignore null.
  arc_window->ClearProperty(ash::kAppIDKey);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));
  // Should ignore uninterested property change.
  arc_window->SetProperty(kNonInterestedPropKey, true);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));
  // Should not ignore non-null value.
  arc_window->SetProperty(ash::kAppIDKey, std::string("app-id"));
  EXPECT_TRUE(IsResizeLockEnabled(arc_window.get()));
}

// Tests that resize lock will not be enabled if the resize lock type is changed
// to RESIZABLE while we're waiting for the valid app id.
TEST_F(ArcResizeLockManagerTest, TestPropertyChangeWithDelayedAppIdCancel) {
  auto arc_window = CreateFakeWindow(true);
  std::string app_id = "app-id";

  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::NONE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  arc_window->SetProperty(ash::kAppIDKey, app_id);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));
}

// Test that resize lock will NOT be enabled for non ARC windows.
TEST_F(ArcResizeLockManagerTest, TestNonArcWindow) {
  auto non_arc_window = CreateFakeWindow(false);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window.get()));
  non_arc_window->SetProperty(
      ash::kArcResizeLockTypeKey,
      ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window.get()));
  non_arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                              ash::ArcResizeLockType::NONE);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window.get()));
}

// Test that the ArcResizeLockState is properly handled for the "first-time
// launch" app (whose state is ArcResizeLockState::READY).
TEST_F(ArcResizeLockManagerTest, ResizeLockStateForFirstTimeLaunch) {
  auto arc_window = CreateFakeWindow(true);
  std::string app_id = "app-id";
  arc_window->SetProperty(ash::kAppIDKey, app_id);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  // Test for RESIZE_DISABLED_TOGGLABLE.
  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::READY);
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
  EXPECT_EQ(pref_delegate()->GetResizeLockState(app_id),
            mojom::ArcResizeLockState::ON);

  // Test for RESIZABLE.
  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::READY);
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::NONE);
  EXPECT_EQ(pref_delegate()->GetResizeLockState(app_id),
            mojom::ArcResizeLockState::READY);

  // Test for RESIZE_DISABLED_NONTOGGLABLE.
  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::READY);
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE);
  EXPECT_EQ(pref_delegate()->GetResizeLockState(app_id),
            mojom::ArcResizeLockState::FULLY_LOCKED);
}

// Tests that metrics for initial resize lock state is recorded correctly.
TEST_F(ArcResizeLockManagerTest, TestMetricsForInitialResizeLockState) {
  std::string app_id_resize_locked = "resize-locked-app-id";
  std::string app_id_non_resize_locked = "non-resize-locked-app-id";
  const auto* initial_state_histogram =
      GetResizeLockStateHistogramNameForTesting(
          ResizeLockStateHistogramType::InitialState);
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount(initial_state_histogram, 0);

  // Not record histogram without the app id ready.
  auto resize_locked_window = CreateFakeWindow(true);
  auto non_resize_locked_window = CreateFakeWindow(true);
  pref_delegate()->SetResizeLockState(app_id_resize_locked,
                                      mojom::ArcResizeLockState::ON);
  histogram_tester.ExpectTotalCount(initial_state_histogram, 0);

  // Record histogram when the app id is ready.
  resize_locked_window->SetProperty(ash::kAppIDKey, app_id_resize_locked);
  histogram_tester.ExpectTotalCount(initial_state_histogram, 1);
  histogram_tester.ExpectBucketCount(initial_state_histogram,
                                     mojom::ArcResizeLockState::ON, 1);
  non_resize_locked_window->SetProperty(ash::kAppIDKey,
                                        app_id_non_resize_locked);
  histogram_tester.ExpectTotalCount(initial_state_histogram, 2);
  histogram_tester.ExpectBucketCount(initial_state_histogram,
                                     mojom::ArcResizeLockState::UNDEFINED, 1);

  // Record histogram only once on initialized.
  pref_delegate()->SetResizeLockState(app_id_resize_locked,
                                      mojom::ArcResizeLockState::OFF);
  histogram_tester.ExpectTotalCount(initial_state_histogram, 2);
}

// Tests that resize shadow type is properly updated according to the resize
// lock type.
TEST_F(ArcResizeLockManagerTest, TestShadowPropertyChange) {
  auto arc_window = CreateFakeWindow(true);
  arc_window->SetProperty(ash::kAppIDKey, std::string("app-id"));

  bool resize_shadow_updated = false;
  ScopedWindowPropertyObserver observer(
      arc_window.get(),
      base::BindLambdaForTesting([&resize_shadow_updated](aura::Window* window,
                                                          const void* key,
                                                          intptr_t old) {
        if (key != ash::kResizeShadowTypeKey)
          return;
        resize_shadow_updated = true;
      }));

  // Unlocked by default.
  EXPECT_EQ(arc_window->GetProperty(ash::kResizeShadowTypeKey),
            ash::ResizeShadowType::kUnlock);

  // Locked for resize locked windows.
  resize_shadow_updated = false;
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
  EXPECT_EQ(arc_window->GetProperty(ash::kResizeShadowTypeKey),
            ash::ResizeShadowType::kLock);
  EXPECT_TRUE(resize_shadow_updated);
  // No redundant property update.
  resize_shadow_updated = false;
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
  EXPECT_FALSE(resize_shadow_updated);
  // Any resize lock type change must trigger resize shadow update.
  resize_shadow_updated = false;
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE);
  EXPECT_TRUE(resize_shadow_updated);

  // Unlocked for non-resize locked windows.
  resize_shadow_updated = false;
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::NONE);
  EXPECT_EQ(arc_window->GetProperty(ash::kResizeShadowTypeKey),
            ash::ResizeShadowType::kUnlock);
  EXPECT_TRUE(resize_shadow_updated);
  // No redundant property update.
  resize_shadow_updated = false;
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::NONE);
  EXPECT_FALSE(resize_shadow_updated);
}

// Tests that the manager works properly even when window gets destroyed.
TEST_F(ArcResizeLockManagerTest, TestWindowDestruction) {
  // Window gets destroyed just after initialization.
  {
    auto arc_window = CreateFakeWindow(true);
    EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));
  }

  // Window gets destroyed after resize lock property change but before getting
  // app id.
  {
    auto arc_window = CreateFakeWindow(true);
    EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));
    arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                            ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
    EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));
  }

  // Window gets destroyed after resize locked.
  {
    auto arc_window = CreateFakeWindow(true);
    EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));
    arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                            ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
    arc_window->SetProperty(ash::kAppIDKey, std::string("app-id"));
    EXPECT_TRUE(IsResizeLockEnabled(arc_window.get()));

    const auto* arc_window_freed_ptr = arc_window.get();
    arc_window.reset();
    // We don't want to hold the freed window ptr.
    EXPECT_FALSE(IsResizeLockEnabled(arc_window_freed_ptr));
  }
}

// Test that UpdateCompatModeButton is called properly according to the property
// or bounds changes.
TEST_F(ArcResizeLockManagerTest, UpdateCompatModeButton) {
  for (const auto initial_type : kArcResizeLockTypes) {
    for (const auto next_type : kArcResizeLockTypes) {
      if (initial_type == next_type)
        continue;
      auto arc_window = CreateFakeWindow(true);

      // Test for initial ArcResizeLockType property.
      EXPECT_FALSE(IsUpdateCompatModeButtonCalled(arc_window.get()));
      arc_window->SetProperty(ash::kArcResizeLockTypeKey, initial_type);
      EXPECT_FALSE(IsUpdateCompatModeButtonCalled(arc_window.get()));
      arc_window->SetProperty(ash::kAppIDKey, std::string("app-id"));
      EXPECT_TRUE(IsUpdateCompatModeButtonCalled(arc_window.get()));

      ResetUpdateCompatModeButtonCalled();

      // Test for ArcResizeLockType property change.
      EXPECT_FALSE(IsUpdateCompatModeButtonCalled(arc_window.get()));
      arc_window->SetProperty(ash::kArcResizeLockTypeKey, next_type);
      EXPECT_TRUE(IsUpdateCompatModeButtonCalled(arc_window.get()));

      ResetUpdateCompatModeButtonCalled();

      // Test for bounds change.
      EXPECT_FALSE(IsUpdateCompatModeButtonCalled(arc_window.get()));
      arc_window->SetBounds(gfx::Rect(0, 0, 10, 30));
      EXPECT_TRUE(IsUpdateCompatModeButtonCalled(arc_window.get()));

      ResetUpdateCompatModeButtonCalled();
    }
  }
}

// Tests that compatible window snapping is properly enabled for resize-locked
// windows.
TEST_F(ArcResizeLockManagerTest, TestCompatWindowSnap) {
  auto arc_window = CreateFakeWindow(true);
  arc_window->SetProperty(ash::kAppIDKey, std::string("app-id"));

  // Resize locking the window makes it compat snappable.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
  const gfx::Size* prop =
      arc_window->GetProperty(ash::kUnresizableSnappedSizeKey);
  EXPECT_TRUE(prop->width() > 0);

  // Non-resize locked window can't be snapped.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::NONE);
  EXPECT_EQ(arc_window->GetProperty(ash::kUnresizableSnappedSizeKey), nullptr);

  // Fully-locked window can't be snapped.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE);
  EXPECT_EQ(arc_window->GetProperty(ash::kUnresizableSnappedSizeKey), nullptr);
}

// Test that the splash screen dialog is shown properly.
TEST_F(ArcResizeLockManagerTest, ShowSplashScreen) {
  auto arc_window = CreateFakeWindow(true);
  std::string app_id = "app-id";
  arc_window->SetProperty(ash::kAppIDKey, app_id);
  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::READY);
  pref_delegate()->SetShowSplashScreenDialogCount(1);

  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  bool show_splash_called = false;
  SetShowSplashCallback(base::BindLambdaForTesting([&](aura::Window* window) {
    show_splash_called = true;
    // The compat-mode button must exist at the time of showing the splash.
    EXPECT_TRUE(IsUpdateCompatModeButtonCalled(window));
  }));

  // Enable resize-lock.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);

  EXPECT_TRUE(show_splash_called);
}

// Test that showing splash screen dialog is not called for game apps.
TEST_F(ArcResizeLockManagerTest, TestGameApp) {
  auto arc_window = CreateFakeWindow(true);
  arc_window->SetProperty(chromeos::kIsGameKey, true);

  const std::string app_id = "app-id";
  arc_window->SetProperty(ash::kAppIDKey, app_id);
  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::READY);
  pref_delegate()->SetShowSplashScreenDialogCount(1);

  EXPECT_FALSE(IsResizeLockEnabled(arc_window.get()));

  bool show_splash_called = false;
  SetShowSplashCallback(base::BindLambdaForTesting(
      [&](aura::Window* window) { show_splash_called = true; }));

  // Enable resize-lock.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);

  EXPECT_FALSE(show_splash_called);
}

}  // namespace arc
