// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CURTAIN_REMOTE_MAINTENANCE_CURTAIN_VIEW_H_
#define ASH_CURTAIN_REMOTE_MAINTENANCE_CURTAIN_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {
class AshWebView;
class OobeDialogUtil;
class StackLayout;
}  // namespace ash

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {
class ViewShadow;
}  // namespace views

namespace ash::curtain {

// ID given to the `AshWebView` that renders the WebUI part of the curtain.
// Used by pixel tests to wait for the WebUI to render.
// (the value is randomly chosen).
constexpr int kRemoteMaintenanceCurtainAshWebViewId = 14569;

// The root view shown as the security curtain overlay when the security curtain
// is created by an enterprise admin through the 'start crd session' remote
// command.
class ASH_EXPORT RemoteMaintenanceCurtainView : public views::View {
  METADATA_HEADER(RemoteMaintenanceCurtainView, views::View)

 public:
  RemoteMaintenanceCurtainView();
  RemoteMaintenanceCurtainView(const RemoteMaintenanceCurtainView&) = delete;
  RemoteMaintenanceCurtainView& operator=(const RemoteMaintenanceCurtainView&) =
      delete;
  ~RemoteMaintenanceCurtainView() override;

 private:
  // `views::View` implementation:
  void OnBoundsChanged(const gfx::Rect&) override;

  void UpdateChildrenSize(const gfx::Size& new_size);

  void Initialize();
  void AddWallpaper();
  void AddCurtainWebView();

  raw_ptr<StackLayout> layout_ = nullptr;
  raw_ptr<AshWebView> curtain_view_ = nullptr;
  raw_ptr<views::View> wallpaper_view_ = nullptr;

  std::unique_ptr<views::ViewShadow> curtain_view_shadow_;
};

}  // namespace ash::curtain

#endif  // ASH_CURTAIN_REMOTE_MAINTENANCE_CURTAIN_VIEW_H_
