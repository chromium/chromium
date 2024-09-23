// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MINI_VIEW_H_
#define ASH_WM_WINDOW_MINI_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class HighlightPathGenerator;
}  // namespace views

namespace ash {
class WindowMiniViewHeaderView;
class WindowPreviewView;

// Defines the interface that extracts the window, visual updates, focus
// installation and update logic to be used or implemented by `WindowMiniView`
// and `GroupContainerCycleView`.
class WindowMiniViewBase : public views::View {
  METADATA_HEADER(WindowMiniViewBase, views::View)

 public:
  WindowMiniViewBase(const WindowMiniViewBase&) = delete;
  WindowMiniViewBase& operator=(const WindowMiniViewBase&) = delete;
  ~WindowMiniViewBase() override;

  bool is_mini_view_focused() const { return is_focused_; }

  // Shows or hides a focus ring around this.
  void UpdateFocusState(bool focus);

  // Returns true if a preview of the given `window` is contained in `this`.
  virtual bool Contains(aura::Window* window) const = 0;

  // If `screen_point` is within the screen bounds of a preview view inside
  // this, returns the window represented by this view, nullptr otherwise.
  virtual aura::Window* GetWindowAtPoint(
      const gfx::Point& screen_point) const = 0;

  // Creates or deletes preview view as needed. For performance reasons, these
  // are not created on construction. Note that this may create or destroy a
  // `WindowPreviewView` which is an expensive operation.
  virtual void SetShowPreview(bool show) = 0;

  // Refreshes the rounded corners and optionally updates the icon view.
  virtual void RefreshItemVisuals() = 0;

  // Try removing the mini view representation of the `destroying_window`.
  // Returns the number of remaining child items that represent windows within
  // `this`. Returns 0, if `destroying_window` is represented by `this` itself
  // rather than a child item.
  virtual int TryRemovingChildItem(aura::Window* destroying_window) = 0;

  // Returns the exposed rounded corners.
  virtual gfx::RoundedCornersF GetRoundedCorners() const = 0;

  // Sets `this` as selected for focus and applies the focus ring around it.
  // Note that `window` is the window that will be activated if `this` is
  // selected while focused. `window` must be contained within `this` (See
  // `Contains()` above).
  virtual void SetSelectedWindowForFocus(aura::Window* window) = 0;

  // Clears the focus on all the windows associated with `this`.
  virtual void ClearFocusSelection() = 0;

 protected:
  WindowMiniViewBase();

  // True if `this` is focused when using keyboard navigation.
  bool is_focused_ = false;
};

// WindowMiniView is a view which contains a header and optionally a mirror of
// the given window. Displaying the mirror is chosen by the subclass by calling
// `SetShowPreview` in their constructors (or later on if they like).
class ASH_EXPORT WindowMiniView : public WindowMiniViewBase,
                                  public aura::WindowObserver {
  METADATA_HEADER(WindowMiniView, WindowMiniViewBase)

 public:
  WindowMiniView(const WindowMiniView&) = delete;
  WindowMiniView& operator=(const WindowMiniView&) = delete;
  ~WindowMiniView() override;

  // The size in dp of the window icon shown on the alt-tab/overview window next
  // to the title.
  static constexpr gfx::Size kIconSize = gfx::Size(24, 24);

  aura::Window* source_window() { return source_window_; }
  const aura::Window* source_window() const { return source_window_; }
  WindowMiniViewHeaderView* header_view() { return header_view_; }
  views::View* backdrop_view() { return backdrop_view_; }
  WindowPreviewView* preview_view() { return preview_view_; }
  const WindowPreviewView* preview_view() const { return preview_view_; }

  // Sets rounded corners on the exposed corners, the inner corners will be
  // sharp.
  void SetRoundedCornersRadius(
      const gfx::RoundedCornersF& exposed_rounded_corners);

  // Sets the visibility of |backdrop_view_|. Creates it if it is null.
  void SetBackdropVisibility(bool visible);

  // Sets or hides rounded corners on `preview_view_`, if it exists.
  void RefreshPreviewRoundedCorners();

  // Updates the rounded corners on `header_view_`, if it exists.
  void RefreshHeaderViewRoundedCorners();

  // Applies the corresponding rounded corners on the focus ring to match the
  // visuals of the `header_view_` and `preview_view_`.
  void RefreshFocusRingVisuals();

  // Resets the preset rounded corners values i.e.
  // `header_view_rounded_corners_` and `preview_view_rounded_corners_`.
  void ResetRoundedCorners();

  // WindowMiniViewBase:
  bool Contains(aura::Window* window) const override;
  aura::Window* GetWindowAtPoint(const gfx::Point& screen_point) const override;
  void SetShowPreview(bool show) override;
  int TryRemovingChildItem(aura::Window* destroying_window) override;
  gfx::RoundedCornersF GetRoundedCorners() const override;
  void SetSelectedWindowForFocus(aura::Window* window) override;
  void ClearFocusSelection() override;
  void Layout(PassKey) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowTitleChanged(aura::Window* window) override;

 protected:
  WindowMiniView(aura::Window* source_window, bool use_custom_focus_predicate);

  // Returns the bounds where the backdrop and preview should go.
  gfx::Rect GetContentAreaBounds() const;

  gfx::Rect GetHeaderBounds() const;

  // Subclasses can override this function to provide customization for margins
  // and layouts of the preview view.
  virtual gfx::Size GetPreviewViewSize() const;

 private:
  // Called when setting the rounded corners to refresh the rounded corners on
  // the `header_view_`, `preview_view_` and focus ring.
  void OnRoundedCornersSet();

  void InstallFocusRing(bool use_custom_predicate);

  void UpdateAccessibleIgnoredState();
  void UpdateAccessibleName();

  // Generates the focus ring path for `this`, which has four rounded corners by
  // default. If this is part of a snap group, the path should match the rounded
  // corners of the `this`.
  std::unique_ptr<views::HighlightPathGenerator> GenerateFocusRingPath();

  // The window this class is meant to be a header for. This class also may
  // optionally show a mirrored view of this window.
  raw_ptr<aura::Window> source_window_;

  // A view that represents the header of `this`.
  raw_ptr<WindowMiniViewHeaderView> header_view_ = nullptr;

  // A view that covers the area except the header. It is null when the window
  // associated is not pillar or letter boxed.
  raw_ptr<views::View> backdrop_view_ = nullptr;

  // Optionally shows a preview of |window_|.
  raw_ptr<WindowPreviewView, DanglingUntriaged> preview_view_ = nullptr;

  // If these optional values are set, they will be used otherwise the default
  // rounded corners will be used.
  std::optional<gfx::RoundedCornersF> exposed_rounded_corners_;
  std::optional<gfx::RoundedCornersF> preview_view_rounded_corners_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MINI_VIEW_H_
