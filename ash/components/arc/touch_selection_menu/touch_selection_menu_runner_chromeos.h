// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TOUCH_SELECTION_MENU_TOUCH_SELECTION_MENU_RUNNER_CHROMEOS_H_
#define ASH_COMPONENTS_ARC_TOUCH_SELECTION_MENU_TOUCH_SELECTION_MENU_RUNNER_CHROMEOS_H_

#include <memory>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window_tracker.h"
#include "ui/views/touchui/touch_selection_menu_runner_views.h"

namespace aura {
class Window;
}

// A Chrome OS TouchSelectionMenuRunner implementation that queries ARC++
// for Smart Text Selection actions based on the current text selection. This
// allows the quick menu to show a new contextual action button.
class TouchSelectionMenuRunnerChromeOS
    : public views::TouchSelectionMenuRunnerViews {
 public:
  TouchSelectionMenuRunnerChromeOS();

  TouchSelectionMenuRunnerChromeOS(const TouchSelectionMenuRunnerChromeOS&) =
      delete;
  TouchSelectionMenuRunnerChromeOS& operator=(
      const TouchSelectionMenuRunnerChromeOS&) = delete;

  ~TouchSelectionMenuRunnerChromeOS() override;

 private:
  // Called asynchronously with the result from the container.
  void OpenMenuWithTextSelectionAction(
      base::WeakPtr<ui::TouchSelectionMenuClient> client,
      const gfx::Rect& anchor_rect,
      const gfx::Size& handle_image_size,
      std::unique_ptr<aura::WindowTracker> tracker,
      std::vector<arc::mojom::TextSelectionActionPtr> actions);

  // Tries to establish connection with ARC to perform text classification. True
  // if a query to ARC was made, false otherwise.
  bool RequestTextSelection(base::WeakPtr<ui::TouchSelectionMenuClient> client,
                            const gfx::Rect& anchor_rect,
                            const gfx::Size& handle_image_size,
                            aura::Window* context);

  // views::TouchSelectionMenuRunnerViews.
  void OpenMenu(base::WeakPtr<ui::TouchSelectionMenuClient> client,
                const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size,
                aura::Window* context) override;

  base::WeakPtrFactory<TouchSelectionMenuRunnerChromeOS> weak_ptr_factory_{
      this};
};

#endif  // ASH_COMPONENTS_ARC_TOUCH_SELECTION_MENU_TOUCH_SELECTION_MENU_RUNNER_CHROMEOS_H_
