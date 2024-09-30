// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PREVIEW_VIEW_H_
#define ASH_WM_WINDOW_PREVIEW_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_mirror_view.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/client/transient_window_client_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// A view that mirrors the client area of a window and all its transient
// descendants.
class ASH_EXPORT WindowPreviewView
    : public views::View,
      public aura::client::TransientWindowClientObserver,
      public aura::WindowObserver {
  METADATA_HEADER(WindowPreviewView, views::View)

 public:
  explicit WindowPreviewView(aura::Window* window);

  WindowPreviewView(const WindowPreviewView&) = delete;
  WindowPreviewView& operator=(const WindowPreviewView&) = delete;

  ~WindowPreviewView() override;

  // Recreate the preview views for the window and all its transient
  // descendants.
  void RecreatePreviews();

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;

  // aura::client::TransientWindowClientObserver:
  void OnTransientChildWindowAdded(aura::Window* parent,
                                   aura::Window* transient_child) override;
  void OnTransientChildWindowRemoved(aura::Window* parent,
                                     aura::Window* transient_child) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;

  aura::Window* window() { return window_; }

 private:
  friend class WindowPreviewViewTestApi;

  void AddWindow(aura::Window* window);
  void RemoveWindow(aura::Window* window);

  // Get the smallest rectangle that contains the bounds of all the windows in
  // |mirror_views_|.
  gfx::RectF GetUnionRect() const;

  raw_ptr<aura::Window> window_;

  base::flat_map<aura::Window*, raw_ptr<WindowMirrorView, CtnExperimental>>
      mirror_views_;

  // Transient children of |window_| may be added as transients before they're
  // actually parented; i.e. `OnTransientChildWindowAdded()` is called before
  // `transient_child->parent()` is set. We track those here so that we can add
  // them to the view once they're parented.
  base::flat_set<raw_ptr<aura::Window, CtnExperimental>>
      unparented_transient_children_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_PREVIEW_VIEW_H_
