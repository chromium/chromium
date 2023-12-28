// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_observer.h"

namespace ash {
namespace {

class WindowDeleter : public aura::WindowObserver {
 public:
  explicit WindowDeleter(aura::Window* window) : target_(window) {}

  WindowDeleter(const WindowDeleter&) = delete;
  WindowDeleter& operator=(const WindowDeleter&) = delete;

  // aura::WindowObserver::
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    delete target_;
  }

 private:
  raw_ptr<aura::Window, DanglingUntriaged> target_;
};

}  // namespace

using RootWindowLayoutManagerTest = AshTestBase;

TEST_F(RootWindowLayoutManagerTest, DeleteChildDuringResize) {
  aura::Window* parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_WallpaperContainer);
  aura::Window* w1 = aura::test::CreateTestWindowWithId(1, parent);
  aura::Window* w2 = aura::test::CreateTestWindowWithId(2, parent);
  WindowDeleter deleter(w1);
  w2->AddObserver(&deleter);
  UpdateDisplay("600x500");
  w2->RemoveObserver(&deleter);
}

}  // namespace ash
