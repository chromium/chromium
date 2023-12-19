// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_POPUP_UTILS_H_
#define ASH_SYSTEM_TRAY_TRAY_POPUP_UTILS_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/login_status.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_ink_drop_style.h"
#include "ash/system/tray/tri_view.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Button;
class ImageView;
class Label;
class LabelButton;
class Painter;
class Separator;
}  // namespace views

namespace ui {
class ImageModel;
}  // namespace ui

namespace ash {
class HoverHighlightView;
class UnfocusableLabel;

// Factory/utility functions used by the system menu.
class ASH_EXPORT TrayPopupUtils {
 public:
  enum class FontStyle {
    // Topmost header rows for default view and detailed view.
    kTitle,
    // Topmost header for secondary tray bubbles
    kPodMenuHeader,
    // Small title used for selections in tray bubbles.
    kSmallTitle,
    // Text in sub-section header rows in detailed views.
    kSubHeader,
    // Main text used by detailed view rows.
    kDetailedViewLabel,
    // System information text (e.g. date/time, battery status, "Scanning for
    // devices..." seen in the Bluetooth detailed view, etc).
    kSystemInfo,
  };

  TrayPopupUtils() = delete;
  TrayPopupUtils(const TrayPopupUtils&) = delete;
  TrayPopupUtils& operator=(const TrayPopupUtils&) = delete;

  // Creates a default container view to be used by system menu rows that are
  // either a single targetable area or not targetable at all. The caller takes
  // over ownership of the created view.
  //
  // The returned view consists of 3 regions: START, CENTER, and END. Any child
  // Views added to the START and END containers will be added horizontally and
  // any Views added to the CENTER container will be added vertically.
  //
  // The START and END containers have a fixed minimum width but can grow into
  // the CENTER container if space is required and available.
  //
  // The CENTER container has a flexible width.
  //
  // `use_wide_layout` uses a wider layout.
  static TriView* CreateDefaultRowView(bool use_wide_layout);

  // Creates a container view to be used by system menu sub-section header rows.
  // The caller takes over ownership of the created view.
  //
  // The returned view contains at least CENTER and END regions having the same
  // properties as when using |CreateMultiTargetRowView|. |start_visible|
  // determines whether the START region should be visible or not. If START is
  // not visible, extra padding is added to the left of the contents.
  //
  // The START (if visible) and END containers have a fixed minimum width but
  // can grow into the CENTER container if space is required and available. The
  // CENTER container has a flexible width.
  //
  // TODO(mohsen): Merge this into TrayDetailedView::AddScrollListSubHeader()
  // once network and VPN also use TrayDetailedView::AddScrollListSubHeader().
  static TriView* CreateSubHeaderRowView(bool start_visible);

  // Creates a container view to be used by system menu rows that want to embed
  // a targetable area within one (or more) of the containers OR by any row
  // that requires a non-default layout within the container views. The returned
  // view will have the following configurations:
  //   - default minimum row height
  //   - default minimum width for the START and END containers
  //   - default left and right insets
  //   - default container flex values
  //   - Each container view will have a FillLayout installed on it
  //
  // The caller takes over ownership of the created view.
  //
  // The START and END containers have a fixed minimum width but can grow into
  // the CENTER container if space is required and available. The CENTER
  // container has a flexible width.
  //
  // Clients can use ConfigureContainer() to configure their own container views
  // before adding them to the returned TriView.
  //
  // `use_wide_layout` uses a wider layout.
  static TriView* CreateMultiTargetRowView(bool use_wide_layout);

  // Returns a label that has been configured for system menu layout. This
  // should be used by all rows that require a label, i.e. both default and
  // detailed rows should use this.
  //
  // TODO(bruthig): Update all system menu rows to use this.
  static views::Label* CreateDefaultLabel();

  // Returns a label that has been configured for system menu layout and does
  // not allow accessibility focus.
  static UnfocusableLabel* CreateUnfocusableLabel();

  // Returns an image view to be used in the main image region of a system menu
  // row. This should be used by all rows that have a main image, i.e. both
  // default and detailed rows should use this.
  //
  // TODO(bruthig): Update all system menu rows to use this.
  //
  // `use_wide_layout` uses a wider layout.
  static views::ImageView* CreateMainImageView(bool use_wide_layout);

  // Creates a default focus painter used for most things in tray popups.
  static std::unique_ptr<views::Painter> CreateFocusPainter();

  // Sets up `view` with the tray detail scroll view header specs.
  static void ConfigureHeader(views::View* view);

  // Sets up `ink_drop` according to jelly ux requirements for row buttons.
  static void ConfigureRowButtonInkdrop(views::InkDropHost* ink_drop);

  // Creates a button for use in the system menu. For MD, this is a prominent
  // text
  // button. For non-MD, this does the same thing as the above. Caller assumes
  // ownership.
  static views::LabelButton* CreateTrayPopupButton(
      views::Button::PressedCallback callback,
      const std::u16string& text);

  // Creates and returns a vertical separator to be used between two items in a
  // material design system menu row. The caller assumes ownership of the
  // returned separator.
  static views::Separator* CreateVerticalSeparator();

  // Installs a HighlightPathGenerator matching the TrayPopupInkDropStyle.
  static void InstallHighlightPathGenerator(
      views::View* host,
      TrayPopupInkDropStyle ink_drop_style);

  // Creates and returns a horizontal separator line to be drawn between rows
  // in a detailed view. If |left_inset| is true, then the separator is inset on
  // the left by the width normally occupied by an icon. Caller assumes
  // ownership of the returned separator.
  static views::Separator* CreateListItemSeparator(bool left_inset);

  // Returns true if it is possible to open WebUI settings in a browser window,
  // i.e. the user is logged in, not on the lock screen, not adding a secondary
  // user, and not in the supervised user creation flow.
  static bool CanOpenWebUISettings();

  // Returns true if it is possible to show the night light feature tile, i.e.
  // the `session_manager::SessionState` is ACTIVE, LOGGED_IN_NOT_ACTIVE, or
  // LOCKED.
  static bool CanShowNightLightFeatureTile();

  // Initializes a row in the system menu as checkable and update the check mark
  // status of this row. If |enterprise_managed| is true, adds an enterprise
  // managed icon to the row.
  static void InitializeAsCheckableRow(HoverHighlightView* container,
                                       bool checked,
                                       bool enterprise_managed);
  // Updates the visibility and a11y state of the checkable row |container|.
  static void UpdateCheckMarkVisibility(HoverHighlightView* container,
                                        bool visible);

  // Updates the color of the checkable row |container|.
  static void UpdateCheckMarkColor(HoverHighlightView* container,
                                   ui::ColorId color_id);

  // Creates the check mark.
  static ui::ImageModel CreateCheckMark(ui::ColorId color_id);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_POPUP_UTILS_H_
