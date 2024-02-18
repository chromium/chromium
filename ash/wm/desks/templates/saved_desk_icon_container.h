// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_CONTAINER_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_CONTAINER_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "ash/wm/desks/templates/saved_desk_icon_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace ui {
class ColorProvider;
}

namespace ash {

class DeskTemplate;

// This class determines which app icons/favicons to show for a saved desk and
// creates the according SavedDeskIconView's. The last SavedDeskIconView in the
// layout is used for storing the overflow count of icons. Not every view in the
// container is visible.
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
  METADATA_HEADER(SavedDeskIconContainer, views::BoxLayoutView)

 public:
  // A struct for storing the various information used to determine which app
  // icons/favicons to display.
  struct IconInfo {
    std::string app_id;
    std::string app_title;
    int activation_index;
    int count;
  };

  using IconIdentifierAndIconInfo =
      std::pair<SavedDeskIconIdentifier, IconInfo>;

  SavedDeskIconContainer();
  SavedDeskIconContainer(const SavedDeskIconContainer&) = delete;
  SavedDeskIconContainer& operator=(const SavedDeskIconContainer&) = delete;
  ~SavedDeskIconContainer() override;

  // The maximum number of icons that can be displayed.
  static constexpr int kMaxIcons = 4;

  // views::BoxLayoutView:
  void Layout(PassKey) override;

  // Given a saved desk, determine which icons to show in this and create
  // the according SavedDeskIconView's.
  void PopulateIconContainerFromTemplate(const DeskTemplate* desk_template);

  // Given `windows`, determine which icons to show in this and create the
  // according SavedDeskIconView's.
  void PopulateIconContainerFromWindows(
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows);

 private:
  // Return a copy of child icon views.
  std::vector<SavedDeskIconView*> GetIconViews() const;

  // Function that runs when a child view finishes loading its icon. Sort child
  // icons and the overflow icon.
  void OnViewLoaded(views::View* icon_view);

  // Sort child icons and update overflow icon.
  void SortIconsAndUpdateOverflowIcon();

  // Sort all child icons using its key. Please note, the overflow icon will
  // always stay at the end. Default icons will always come after non-default
  // icons. The order of non-default icons will be stable, i.e. it preserves the
  // original order.
  void SortIcons();

  // Update icon visibility and the overflow icon depending on the available
  // space.
  void UpdateOverflowIcon();

  // Given a sorted vector of pairs of icon identifier and icon info, create
  // views for them.
  void CreateIconViewsFromIconIdentifiers(
      const std::vector<IconIdentifierAndIconInfo>&
          icon_identifier_to_icon_info);

  // Pointer of the overflow icon view.
  raw_ptr<views::View> overflow_icon_view_ = nullptr;

  // Number of apps that are not shown as icons in the container.
  int uncreated_app_count_ = 0;

  // If `this` is created with an incognito window, store the ui::ColorProvider
  // of one of the incognito windows to retrieve its icon's color.
  raw_ptr<const ui::ColorProvider> incognito_window_color_provider_ = nullptr;

  base::WeakPtrFactory<SavedDeskIconContainer> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */,
                   SavedDeskIconContainer,
                   views::BoxLayoutView)
VIEW_BUILDER_METHOD(PopulateIconContainerFromTemplate, const DeskTemplate*)
VIEW_BUILDER_METHOD(
    PopulateIconContainerFromWindows,
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>&)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SavedDeskIconContainer)

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ICON_CONTAINER_H_
