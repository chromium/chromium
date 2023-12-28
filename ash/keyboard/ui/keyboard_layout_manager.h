// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_KEYBOARD_LAYOUT_MANAGER_H_
#define ASH_KEYBOARD_UI_KEYBOARD_LAYOUT_MANAGER_H_

#include "ash/keyboard/ui/keyboard_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"

namespace keyboard {

class KeyboardUIController;

// LayoutManager for the virtual keyboard container. Manages a single window
// (the virtual keyboard) and keeps it positioned at the bottom of the
// owner window.
class KEYBOARD_EXPORT KeyboardLayoutManager : public aura::LayoutManager {
 public:
  explicit KeyboardLayoutManager(KeyboardUIController* controller);

  KeyboardLayoutManager(const KeyboardLayoutManager&) = delete;
  KeyboardLayoutManager& operator=(const KeyboardLayoutManager&) = delete;

  ~KeyboardLayoutManager() override;

  // Overridden from aura::LayoutManager
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  raw_ptr<KeyboardUIController> controller_;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_KEYBOARD_LAYOUT_MANAGER_H_
