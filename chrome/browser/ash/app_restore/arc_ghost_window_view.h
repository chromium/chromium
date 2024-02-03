// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_VIEW_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace arc {
enum class GhostWindowType;
}

namespace ash::full_restore {

class ArcGhostWindowShellSurface;

// ID for different component view in ArcGhostWindowView.
enum ContentID {
  ID_NONE = 0,
  ID_ICON_IMAGE,
  ID_THROBBER,
  ID_MESSAGE_LABEL,
};

// The view of ARC ghost window content. It shows the icon of app and a
// throbber. It is used on ARC ghost window shell surface overlay, so it will
// be destroyed after actual ARC task window launched.
class ArcGhostWindowView : public views::View {
  METADATA_HEADER(ArcGhostWindowView, views::View)

 public:
  ArcGhostWindowView(ArcGhostWindowShellSurface* shell_surface,
                     const std::string& app_name);
  ArcGhostWindowView(const ArcGhostWindowView&) = delete;
  ArcGhostWindowView operator=(const ArcGhostWindowView&) = delete;
  ~ArcGhostWindowView() override;

  // The original style of ghost window requires the App theme color.
  void SetThemeColor(uint32_t theme_color);

  // Initialize or replace content of ghost window. If use the original style,
  // the theme color should be set before call this function.
  void SetGhostWindowViewType(arc::GhostWindowType type);

  // Load icon from App service by app id.
  void LoadIcon(const std::string& app_id);

  // views::View:
  void OnThemeChanged() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ArcGhostWindowViewTest, IconLoadTest);
  FRIEND_TEST_ALL_PREFIXES(ArcGhostWindowViewTest, EmptyViewIconLoadTest);
  FRIEND_TEST_ALL_PREFIXES(ArcGhostWindowViewTest, FixupMessageTest);

  // Callback function for loading icon from App service.
  void OnIconLoaded(apps::IconValuePtr icon_value);

  void AddCommonChildrenViews();
  void AddChildrenViewsForFixupType();
  void AddChildrenViewsForAppLaunchType();

  uint32_t theme_color_;
  std::string app_name_;
  gfx::ImageSkia icon_raw_data_;
  arc::GhostWindowType ghost_window_type_;

  raw_ptr<ArcGhostWindowShellSurface> shell_surface_ = nullptr;
  base::OnceCallback<void(apps::IconValuePtr icon_value)>
      icon_loaded_cb_for_testing_;

  base::WeakPtrFactory<ArcGhostWindowView> weak_ptr_factory_{this};
};

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_VIEW_H_
