// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_WINDOW_HIDER_H_
#define ASH_GLANCEABLES_GLANCEABLES_WINDOW_HIDER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ui/aura/window_observer.h"

namespace ash {

// Scoped object that hides all windows in the MRU list when created. Restores
// all the windows when deallocated. Implements aura::WindowObserver to detect
// when windows are destroyed while hidden.
class ASH_EXPORT GlanceablesWindowHider : public aura::WindowObserver {
 public:
  GlanceablesWindowHider();
  GlanceablesWindowHider(const GlanceablesWindowHider&) = delete;
  GlanceablesWindowHider& operator=(const GlanceablesWindowHider&) = delete;
  ~GlanceablesWindowHider() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Windows are stored in stacking order, lowest window in front.
  std::vector<aura::Window*> windows_;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_WINDOW_HIDER_H_
