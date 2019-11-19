// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/arc/arc_input_method_surface_manager.h"

#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"

namespace ash {
namespace {

class TestArcInputMethodSurfaceManagerObserver
    : public ArcInputMethodSurfaceManager::Observer {
 public:
  TestArcInputMethodSurfaceManagerObserver() = default;
  ~TestArcInputMethodSurfaceManagerObserver() override = default;

  void OnArcInputMethodSurfaceBoundsChanged(const gfx::Rect& bounds) override {
    ++bounds_changed_calls_;
    last_bounds_ = bounds;
  }

  int bounds_changed_calls_ = 0;
  gfx::Rect last_bounds_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestArcInputMethodSurfaceManagerObserver);
};

}  // namespace

class ArcInputMethodSurfaceManagerTest : public AshTestBase {
 public:
  ArcInputMethodSurfaceManagerTest() = default;
  ~ArcInputMethodSurfaceManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    wm_helper_ = std::make_unique<exo::WMHelperChromeOS>();
    exo::WMHelper::SetInstance(wm_helper_.get());
  }

  void TearDown() override {
    exo::WMHelper::SetInstance(nullptr);
    wm_helper_.reset();
    AshTestBase::TearDown();
  }

 private:
  std::unique_ptr<exo::WMHelper> wm_helper_;

  DISALLOW_COPY_AND_ASSIGN(ArcInputMethodSurfaceManagerTest);
};

TEST_F(ArcInputMethodSurfaceManagerTest, AddRemoveSurface) {
  ArcInputMethodSurfaceManager manager;
  EXPECT_EQ(nullptr, manager.GetSurface());
  auto surface = std::make_unique<exo::Surface>();
  auto input_method_surface =
      std::make_unique<exo::InputMethodSurface>(nullptr, surface.get(), 1.0);
  manager.AddSurface(input_method_surface.get());
  EXPECT_EQ(input_method_surface.get(), manager.GetSurface());
  manager.RemoveSurface(input_method_surface.get());
  EXPECT_EQ(nullptr, manager.GetSurface());
}

TEST_F(ArcInputMethodSurfaceManagerTest, Observer) {
  ArcInputMethodSurfaceManager manager;
  EXPECT_EQ(nullptr, manager.GetSurface());
  auto surface = std::make_unique<exo::Surface>();
  auto input_method_surface =
      std::make_unique<exo::InputMethodSurface>(&manager, surface.get(), 1.0);
  surface->SetViewport(gfx::Size(500, 500));
  surface->Commit();

  gfx::Rect test_bounds(10, 10, 100, 100);
  TestArcInputMethodSurfaceManagerObserver observer;
  manager.AddObserver(&observer);
  ASSERT_EQ(0, observer.bounds_changed_calls_);

  surface->SetInputRegion(test_bounds);
  surface->Commit();

  EXPECT_EQ(1, observer.bounds_changed_calls_);
  EXPECT_EQ(test_bounds, observer.last_bounds_);

  surface->SetInputRegion(gfx::Rect());
  surface->Commit();

  EXPECT_EQ(2, observer.bounds_changed_calls_);
  EXPECT_EQ(gfx::Rect(), observer.last_bounds_);
}

}  // namespace ash
