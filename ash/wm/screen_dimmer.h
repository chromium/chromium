// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCREEN_DIMMER_H_
#define ASH_WM_SCREEN_DIMMER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"

namespace ash {

class ScreenDimmerTest;
class WindowDimmer;

template <typename UserData>
class WindowUserData;

// ScreenDimmer displays a partially-opaque layer above everything
// else in the given container window to darken the display.  It shouldn't be
// used for long-term brightness adjustments due to performance
// considerations -- it's only intended for cases where we want to
// briefly dim the screen (e.g. to indicate to the user that we're
// about to suspend a machine that lacks an internal backlight that
// can be adjusted).
class ASH_EXPORT ScreenDimmer : public ShellObserver {
 public:
  // Indicates the container ScreenDimmer operates on.
  enum class Container {
    ROOT,
    LOCK_SCREEN,
  };

  explicit ScreenDimmer(Container container);

  ScreenDimmer(const ScreenDimmer&) = delete;
  ScreenDimmer& operator=(const ScreenDimmer&) = delete;

  ~ScreenDimmer() override;

  // Dim or undim the layers.
  void SetDimming(bool should_dim);

  void set_at_bottom(bool at_bottom) { at_bottom_ = at_bottom; }

  bool is_dimming() const { return is_dimming_; }

  // Find a ScreenDimmer in the container, or nullptr if it does not exist.
  static ScreenDimmer* FindForTest(int container_id);

 private:
  friend class ScreenDimmerTest;

  // Returns the aura::Windows (one per display) that correspond to
  // |container_|.
  std::vector<aura::Window*> GetAllContainers();

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;

  // Update the dimming state. This will also create a new DimWindow
  // if necessary. (Used when a new display is connected)
  void Update(bool should_dim);

  const Container container_;

  // Are we currently dimming the screen?
  bool is_dimming_;
  bool at_bottom_;

  // Owns the WindowDimmers.
  std::unique_ptr<WindowUserData<WindowDimmer>> window_dimmers_;
};

}  // namespace ash

#endif  // ASH_WM_SCREEN_DIMMER_H_
