// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WINDOW_PREVIEW_BUBBLE_H_
#define ASH_SHELF_SHELF_WINDOW_PREVIEW_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_bubble.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {
class Event;
class LocatedEvent;
}  // namespace ui

namespace views {
class MenuItemView;
class View;
}  // namespace views

namespace ash {

class WindowPreviewView;

// A bubble dialog view that displays a mirror preview of an aura::Window
// anchored to a shelf tooltip or menu item.
//
// Input is handled via pre-target handlers (`set_accept_events(false)`) so
// events are received while the app menu holds capture, across anchor-preview
// gaps, and before dismissal deletes the bubble.
class ASH_EXPORT ShelfWindowPreviewBubble : public ShelfBubble,
                                            public aura::WindowObserver {
  METADATA_HEADER(ShelfWindowPreviewBubble, ShelfBubble)

 public:
  ShelfWindowPreviewBubble(views::MenuItemView* anchor,
                           aura::Window* window,
                           base::OnceClosure dismiss_callback);
  ShelfWindowPreviewBubble(ShelfBubble* anchor,
                           aura::Window* window,
                           base::OnceClosure dismiss_callback);

  ShelfWindowPreviewBubble(const ShelfWindowPreviewBubble&) = delete;
  ShelfWindowPreviewBubble& operator=(const ShelfWindowPreviewBubble&) = delete;

  ~ShelfWindowPreviewBubble() override;

  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Updates the anchor view and mirrored window in place without recreating
  // the bubble widget.
  void UpdateAnchorAndWindow(views::MenuItemView* anchor, aura::Window* window);

  // Fades out the preview bubble's widget and closes it when the animation
  // finishes.
  void FadeOutAndClose();

  // Returns true if `screen_point` is within the pairwise hover bridges
  // between this preview bubble, its anchor, and (for tooltip-anchored
  // previews) the underlying shelf item, so the mouse can travel between them
  // without closing the preview.
  bool IsPointInPreviewRegion(const gfx::Point& screen_point) const;

  aura::Window* window() const { return window_; }

 private:
  ShelfWindowPreviewBubble(views::View* anchor,
                           aura::Window* window,
                           views::BubbleBorder::Arrow arrow,
                           base::OnceClosure dismiss_callback);

  void AddPreTargetHandler();
  void RemovePreTargetHandler();
  // Dismisses the preview and activates `window_` if `event` is over the
  // bubble. May synchronously delete `this`.
  void ProcessPressedEvent(ui::LocatedEvent* event);

  void CreatePreviewView();
  void RemovePreviewView();

  raw_ptr<aura::Window> window_ = nullptr;
  raw_ptr<WindowPreviewView> preview_view_ = nullptr;
  base::OnceClosure dismiss_callback_;
  bool is_pre_target_handler_ = false;
  // Tracks the target window of a touch sequence that started over this bubble
  // so subsequent touches and gestures on that consumer are consumed.
  base::WeakPtr<aura::Window> gesture_consumer_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WINDOW_PREVIEW_BUBBLE_H_
