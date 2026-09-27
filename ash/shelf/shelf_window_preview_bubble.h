// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WINDOW_PREVIEW_BUBBLE_H_
#define ASH_SHELF_SHELF_WINDOW_PREVIEW_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_bubble.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace aura {
class Window;
}

namespace views {
class View;
}

namespace ash {

class WindowPreviewView;

// A bubble dialog view that displays a mirror preview of an aura::Window
// anchored to a shelf item or menu item.
class ASH_EXPORT ShelfWindowPreviewBubble : public ShelfBubble,
                                            public aura::WindowObserver {
  METADATA_HEADER(ShelfWindowPreviewBubble, ShelfBubble)

 public:
  ShelfWindowPreviewBubble(views::View* anchor, aura::Window* window);

  ShelfWindowPreviewBubble(const ShelfWindowPreviewBubble&) = delete;
  ShelfWindowPreviewBubble& operator=(const ShelfWindowPreviewBubble&) = delete;

  ~ShelfWindowPreviewBubble() override;

  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Updates the anchor view and mirrored window in place without recreating
  // the bubble widget.
  void UpdateAnchorAndWindow(views::View* anchor, aura::Window* window);

  // Fades out the preview bubble's widget and closes it when the animation
  // finishes.
  void FadeOutAndClose();

  aura::Window* window() const { return window_; }

 private:
  void CreatePreviewView();
  void RemovePreviewView();

  raw_ptr<aura::Window> window_ = nullptr;
  raw_ptr<WindowPreviewView> preview_view_ = nullptr;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WINDOW_PREVIEW_BUBBLE_H_
