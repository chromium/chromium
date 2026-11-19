// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCOPED_DECORATION_HIDER_H_
#define ASH_WM_SCOPED_DECORATION_HIDER_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Hides the decoration (e.g. shadow, highlight border) of a window while this
// object is alive. The decoration stays hidden until all the objects created
// for the window are destroyed.
class ASH_EXPORT ScopedDecorationHider {
 public:
  explicit ScopedDecorationHider(aura::Window* window);
  ScopedDecorationHider(ScopedDecorationHider&& other) noexcept;
  ScopedDecorationHider& operator=(ScopedDecorationHider&& other) noexcept;
  ScopedDecorationHider(const ScopedDecorationHider&) = delete;
  ScopedDecorationHider& operator=(const ScopedDecorationHider&) = delete;
  ~ScopedDecorationHider();

  const aura::Window* window() const { return window_.get(); }

 private:
  void Reset();
  void UpdateCount(int delta);

  base::WeakPtr<aura::Window> window_;
};

}  // namespace ash

#endif  // ASH_WM_SCOPED_DECORATION_HIDER_H_
