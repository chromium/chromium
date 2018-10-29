// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_VIRTUAL_KEYBOARD_MODEL_H_
#define ASH_SYSTEM_MODEL_VIRTUAL_KEYBOARD_MODEL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/keyboard/arc/arc_input_method_surface_manager.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class ArcInputMethodSurfaceManager;

// Model to store virtual keyboard visibility state.
class ASH_EXPORT VirtualKeyboardModel
    : public ArcInputMethodSurfaceManager::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    virtual void OnVirtualKeyboardVisibilityChanged() = 0;
  };

  VirtualKeyboardModel();
  ~VirtualKeyboardModel() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Start/stop observing ArcInputMethodSurfaceManager.
  void SetInputMethodSurfaceManagerObserver(
      ArcInputMethodSurfaceManager* input_method_surface_manager);
  void RemoveInputMethodSurfaceManagerObserver(
      ArcInputMethodSurfaceManager* input_method_surface_manager);

  // ArcInputMethodSurfaceManager::Observer:
  void OnArcInputMethodSurfaceBoundsChanged(const gfx::Rect& bounds) override;

  bool visible() const { return visible_; }

 private:
  void NotifyChanged();

  // The visibility of virtual keyboard.
  bool visible_ = false;

  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardModel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_VIRTUAL_KEYBOARD_MODEL_H_
