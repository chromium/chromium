// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCOPED_WINDOWS_MOVER_H_
#define ASH_WM_SCOPED_WINDOWS_MOVER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"

namespace ash {

// A utility class to move given windows to the specified display.
class ScopedWindowsMover {
 public:
  ScopedWindowsMover(int64_t dest_display_id);
  ScopedWindowsMover(const ScopedWindowsMover&) = delete;
  ScopedWindowsMover& operator=(const ScopedWindowsMover&) = delete;
  ~ScopedWindowsMover();

  void add_window(aura::Window* window) { windows_.push_back(window); }

  // Sets the callback that will be called after all windows are moved.
  void set_callback(base::OnceClosure callback) {
    moved_callback_ = std::move(callback);
  }

  aura::Window::Windows& windows() { return windows_; }

 private:
  // Destination display's id.
  int64_t dest_display_id_;
  aura::Window::Windows windows_;
  base::OnceClosure moved_callback_;
};

}  // namespace ash

#endif  // ASH_WM_SCOPED_WINDOWS_MOVER_H_
