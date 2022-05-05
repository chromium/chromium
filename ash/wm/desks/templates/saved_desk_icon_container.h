// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_CONTAINER_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_CONTAINER_H_

#include <string>
#include <utility>
#include <vector>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ui {
class ColorProvider;
}

namespace ash {

class DeskTemplate;
class SavedDeskIconView;

// This class for determines which app icons/favicons to show for a desk
// template and creates the according SavedDeskIconView's for them.
// The last SavedDeskIconView in the layout is used for storing the
// overflow count of icons. Not every view in the container is visible.
//   _______________________________________________________________________
//   |  _________  _________   _________________   _________   _________   |
//   |  |       |  |       |   |       |       |   |       |   |       |   |
//   |  |   I   |  |   I   |   |   I      + N  |   |   I   |   |  + N  |   |
//   |  |_______|  |_______|   |_______|_______|   |_______|   |_______|   |
//   |_____________________________________________________________________|
//
// If there are multiple apps associated with a certain icon, the icon is drawn
// once with a +N label attached, up to +99. If there are too many icons to be
// displayed within the given width, we draw as many and a label at the end that
// says +N, up to +99.
class SavedDeskIconContainer : public views::BoxLayoutView {
 public:
  METADATA_HEADER(SavedDeskIconContainer);

  // A struct for storing the various information used to determine which app
  // icons/favicons to display.
  struct IconInfo {
    std::string app_id;
    std::string app_title;
    int activation_index;
    int count;
  };

  using IconIdentifierAndIconInfo = std::pair<std::string, IconInfo>;

  SavedDeskIconContainer();
  SavedDeskIconContainer(const SavedDeskIconContainer&) = delete;
  SavedDeskIconContainer& operator=(const SavedDeskIconContainer&) = delete;
  ~SavedDeskIconContainer() override;

  // The maximum number of icons that can be displayed.
  static constexpr int kMaxIcons = 4;

  const ui::ColorProvider* incognito_window_color_provider() const {
    return incognito_window_color_provider_;
  }

  // Given a saved desk, determine which icons to show in this and create
  // the according SavedDeskIconView's.
  void PopulateIconContainerFromTemplate(DeskTemplate* desk_template);

  // Given `windows`, determine which icons to show in this and create the
  // according SavedDeskIconView's.
  void PopulateIconContainerFromWindows(
      const std::vector<aura::Window*>& windows);

  // views::BoxLayoutView:
  void Layout() override;

 private:
  // Given a sorted vector of pairs of icon identifier and icon info, create
  // views for them.
  void CreateIconViewsFromIconIdentifiers(
      const std::vector<IconIdentifierAndIconInfo>&
          icon_identifier_to_icon_info);

  // If `this` is created with an incognito window, store the ui::ColorProvider
  // of one of the incognito windows to retrieve its icon's color.
  const ui::ColorProvider* incognito_window_color_provider_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   SavedDeskIconContainer,
                   views::BoxLayoutView)
VIEW_BUILDER_METHOD(PopulateIconContainerFromTemplate, DeskTemplate*)
VIEW_BUILDER_METHOD(PopulateIconContainerFromWindows,
                    const std::vector<aura::Window*>&)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SavedDeskIconContainer)

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_CONTAINER_H_
