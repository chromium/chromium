// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_OVERVIEW_TEST_API_H_
#define ASH_PUBLIC_CPP_OVERVIEW_TEST_API_H_

#include <cstdint>

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ash {

enum class OverviewAnimationState : int32_t {
  kEnterAnimationComplete,
  kExitAnimationComplete,
};

struct OverviewItemInfo {
  // A window represented by an overview item.
  aura::Window* window = nullptr;

  // Screen bounds of an overview item.
  gfx::Rect bounds_in_screen;

  // Whether an overview item is being dragged.
  bool is_dragged = false;
};

// Overview item info keyed by app windows.
using OverviewInfo = base::flat_map<aura::Window*, OverviewItemInfo>;

// Provides access to the limited functions of OverviewController for autotest
// private API.
class ASH_EXPORT OverviewTestApi {
 public:
  OverviewTestApi();
  ~OverviewTestApi();

  using DoneCallback = base::OnceCallback<void(bool)>;

  // Set the overview mode state to |start|. Calls |done_callback| when it is
  // done. |done_callback| will be called with a boolean if the animation
  // is completed or canceled. If the overview mode state is already same to
  // |start|, it does nothing but invokes |done_callback| with true.
  void SetOverviewMode(bool start, DoneCallback done_callback);

  // Returns overview info for the current overview items if overview is
  // started. Otherwise, returns base::nullopt;
  base::Optional<OverviewInfo> GetOverviewInfo() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewTestApi);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_OVERVIEW_TEST_API_H_
