// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MINI_VIEW_H_
#define ASH_WM_WINDOW_MINI_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class Label;
class View;
}  // namespace views

namespace ash {
class WindowPreviewView;

// WindowMiniView is a view which contains a header and optionally a mirror of
// the given window. Displaying the mirror is chosen by the subclass by calling
// |SetShowPreview| in their constructors (or later on if they like).
class ASH_EXPORT WindowMiniView : public views::View,
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
  // Padding between header items.
  static constexpr int kHeaderPaddingDp = 12;

  // The corner radius for WindowMiniView. Please notice, instead of setting the
  // corner radius directly on the window mini view, setting the corner radius
  // on its children (header view, preview header). The reasons are:
  // 1. The WindowMiniView might have a non-empty border.
  // 2. The focus ring which is a child view of the WindowMiniView couldn't be
  // drawn correctly if its parent's layer is clipped.
  static constexpr int kWindowMiniViewCornerRadius = 16;

  // Sets the visibility of |backdrop_view_|. Creates it if it is null.
  void SetBackdropVisibility(bool visible);

  // Creates or deletes |preview_view_| as needed.
  void SetShowPreview(bool show);

  // Sets or hides rounded corners on |preview_view_|, if it exists.
  void UpdatePreviewRoundedCorners(bool show);

  // Shows or hides a focus ring around this view.
  void UpdateFocusState(bool focus);

  views::View* header_view() { return header_view_; }
  views::Label* title_label() const { return title_label_; }
  views::ImageView* icon_view() { return icon_view_; }
  views::View* backdrop_view() { return backdrop_view_; }
  WindowPreviewView* preview_view() const { return preview_view_; }

 protected:
  explicit WindowMiniView(aura::Window* source_window, int border_inset = 0);

  // Updates the icon view by creating it if necessary, and grabbing the correct
  // image from |source_window_|.
  void UpdateIconView();

  // Returns the bounds where the backdrop and preview should go.
  gfx::Rect GetContentAreaBounds() const;

  // Subclasses can override these functions to provide customization for
  // margins and layouts of certain elements.
  virtual gfx::Rect GetHeaderBounds() const;
  virtual gfx::Size GetPreviewViewSize() const;

  // views::View:
  void Layout() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowTitleChanged(aura::Window* window) override;

  aura::Window* source_window() const { return source_window_; }

 private:
  // The window this class is meant to be a header for. This class also may
  // optionally show a mirrored view of this window.
  raw_ptr<aura::Window, ExperimentalAsh> source_window_;

  // Views for the icon and title.
  raw_ptr<views::View, ExperimentalAsh> header_view_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> title_label_ = nullptr;
  raw_ptr<views::ImageView, ExperimentalAsh> icon_view_ = nullptr;

  // A view that covers the area except the header. It is null when the window
  // associated is not pillar or letter boxed.
  raw_ptr<views::View, ExperimentalAsh> backdrop_view_ = nullptr;

  // Optionally shows a preview of |window_|.
  raw_ptr<WindowPreviewView, ExperimentalAsh> preview_view_ = nullptr;

  // True if the window mini view is focused when using keyboard navigation.
  bool is_focused_ = false;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MINI_VIEW_H_
