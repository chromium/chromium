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

namespace ash {
class WindowMiniViewHeaderView;
class WindowPreviewView;

// Defines the interface that extracts the window, visual updates, focus
// installation and update logic to be used or implemented by `WindowMiniView`
// and `GroupContainerCycleView`.
class WindowMiniViewBase : public views::View {
 public:
  METADATA_HEADER(WindowMiniViewBase);

  WindowMiniViewBase(const WindowMiniViewBase&) = delete;
  WindowMiniViewBase& operator=(const WindowMiniViewBase&) = delete;
  ~WindowMiniViewBase() override;

  // Shows or hides a focus ring around this.
  void UpdateFocusState(bool focus);

  // Sets rounded corners on the exposed corners, the inner corners will be
  // sharp.
  void SetRoundedCornersRadius(
      const gfx::RoundedCornersF& exposed_rounded_corners);

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

 protected:
  WindowMiniViewBase();

  // If these optional values are set, the preset rounded corners will be used
  // otherwise the default rounded corners will be used.
  absl::optional<gfx::RoundedCornersF> header_view_rounded_corners_;
  absl::optional<gfx::RoundedCornersF> preview_view_rounded_corners_;

 private:
  void InstallFocusRing();

  // True if this view is focused when using keyboard navigation.
  bool is_focused_ = false;
};

// WindowMiniView is a view which contains a header and optionally a mirror of
// the given window. Displaying the mirror is chosen by the subclass by calling
// `SetShowPreview` in their constructors (or later on if they like).
class ASH_EXPORT WindowMiniView : public WindowMiniViewBase,
                                  public aura::WindowObserver {
 public:
  METADATA_HEADER(WindowMiniView);

  WindowMiniView(const WindowMiniView&) = delete;
  WindowMiniView& operator=(const WindowMiniView&) = delete;
  ~WindowMiniView() override;

  static constexpr int kHeaderHeightDp = 40;
  // The size in dp of the window icon shown on the alt-tab/overview window next
  // to the title.
  static constexpr gfx::Size kIconSize = gfx::Size(24, 24);

  // The corner radius for WindowMiniView. Note that instead of setting the
  // corner radius directly on the window mini view, setting the corner radius
  // on its children (header view, preview header). The reasons are:
  // 1. The WindowMiniView might have a non-empty border.
  // 2. The focus ring which is a child view of the WindowMiniView couldn't be
  // drawn correctly if its parent's layer is clipped.
  static constexpr int kWindowMiniViewCornerRadius = 16;

  aura::Window* source_window() { return source_window_; }
  const aura::Window* source_window() const { return source_window_; }
  WindowMiniViewHeaderView* header_view() { return header_view_; }
  views::View* backdrop_view() { return backdrop_view_; }
  WindowPreviewView* preview_view() { return preview_view_; }
  const WindowPreviewView* preview_view() const { return preview_view_; }

  // Sets the visibility of |backdrop_view_|. Creates it if it is null.
  void SetBackdropVisibility(bool visible);

  // Sets or hides rounded corners on `preview_view_`, if it exists.
  void RefreshPreviewRoundedCorners(bool show);

  // Updates the rounded corners on `header_view_`, if it exists.
  void RefreshHeaderViewRoundedCorners();

  // Resets the preset rounded corners values i.e.
  // `header_view_rounded_corners_` and `preview_view_rounded_corners_`.
  void ResetRoundedCorners();

  // WindowMiniViewBase:
  bool Contains(aura::Window* window) const override;
  aura::Window* GetWindowAtPoint(const gfx::Point& screen_point) const override;
  void SetShowPreview(bool show) override;
  int TryRemovingChildItem(aura::Window* destroying_window) override;
  gfx::RoundedCornersF GetRoundedCorners() const override;

 protected:
  explicit WindowMiniView(aura::Window* source_window);

  // Returns the bounds where the backdrop and preview should go.
  gfx::Rect GetContentAreaBounds() const;

  // Subclasses can override these functions to provide customization for
  // margins and layouts of certain elements.
  virtual gfx::Rect GetHeaderBounds() const;
  virtual gfx::Size GetPreviewViewSize() const;

  // views::View:
  void Layout() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowTitleChanged(aura::Window* window) override;

 private:
  // The window this class is meant to be a header for. This class also may
  // optionally show a mirrored view of this window.
  raw_ptr<aura::Window, ExperimentalAsh> source_window_;

  // A view that represents the header of `this`.
  raw_ptr<WindowMiniViewHeaderView, ExperimentalAsh> header_view_ = nullptr;

  // A view that covers the area except the header. It is null when the window
  // associated is not pillar or letter boxed.
  raw_ptr<views::View, ExperimentalAsh> backdrop_view_ = nullptr;

  // Optionally shows a preview of |window_|.
  raw_ptr<WindowPreviewView, DanglingUntriaged | ExperimentalAsh>
      preview_view_ = nullptr;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MINI_VIEW_H_
