// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_TILE_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_TILE_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class ImageButton;
class ImageView;
class InkDropContainerView;
class Label;
}  // namespace views

namespace ash {

// The main button used in FeatureTilesContainerView, which acts as an entry
// point for features in QuickSettingsView.

// There are two TileTypes: Primary and Compact.

// The primary tile has an icon and title, and may have a subtitle. The icon may
// or may not be separately clickable. The tile has one of the following
// behaviors:
// 1. Launch surface        (e.g. Screen Capture)
// 2. Toggle                (e.g. Toggle Dark Theme)
// 3. Drill-in              (e.g. Go to Accessibility detailed view)
// 4. Toggle with drill-in  (e.g. Toggle Wi-Fi | Go to Network details)
// 5. Togglable tile with decorative drill-in (e.g. Selecting a VPN network)

// The compact tile has an icon and a single title, which may be
// multi-line. They are always placed in pairs side by side to take up the
// space of a regular FeatureTile. Regular tiles may switch to their compact
// version when necessary, e.g. when entering TabletMode. It presents one
// of the following behaviors:
// 1. Launch surface        (e.g. Screen Capture)
// 2. Toggle                (e.g. Toggle Auto-rotate)
// 3. Drill-in              (e.g. Go to Cast detailed view)
class ASH_EXPORT FeatureTile : public views::Button {
 public:
  METADATA_HEADER(FeatureTile);

  // Used in the FeatureTile constructor to set the tile view type.
  enum class TileType {
    kPrimary = 0,
    kCompact = 1,
    kMaxValue = kCompact,
  };

  // Constructor for FeatureTiles. `callback` will be called when interacting
  // with the main part of the button, which accounts for the whole tile.
  // If the icon is not separately clickable (the default), `callback` will
  // also be called when clicking on the icon.
  explicit FeatureTile(base::RepeatingCallback<void()> callback,
                       bool is_togglable = true,
                       TileType type = TileType::kPrimary);
  FeatureTile(const FeatureTile&) = delete;
  FeatureTile& operator=(const FeatureTile&) = delete;
  ~FeatureTile() override;

  // Creates child views of Feature Tile. The constructed view will vary
  // depending on the button's `type_`.
  void CreateChildViews();

  // Sets whether the icon on the left is clickable, separate from clicking on
  // the tile itself. Use SetIconClickCallback() to set the callback. This
  // function is separate from SetIconClickCallback() because it's likely that
  // FeatureTile users will want to set the callback once but may want to switch
  // the icon between being clickable or not (e.g. the network icon based on
  // Ethernet vs. Wi-Fi).
  void SetIconClickable(bool clickable);

  // Sets the `callback` for clicks on `icon_button_`.
  void SetIconClickCallback(base::RepeatingCallback<void()> callback);

  // Creates a decorative `drill_in_arrow_` on the right side of the tile. This
  // indicates to the user that the tile shows a detailed view when pressed.
  void CreateDecorativeDrillInArrow();

  TileType tile_type() { return type_; }

  // Updates the colors of the background and elements of the button.
  void UpdateColors();

  // Updates the `toggled_` state of the tile. If the tile is not togglable,
  // `toggled_` will always be false.
  void SetToggled(bool toggled);
  bool IsToggled() const;

  // Sets the vector icon.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Sets the button's background color or toggled color with color ID when the
  // button wants to have a different background color from the default one.
  void SetBackgroundColorId(ui::ColorId background_color_id);
  void SetBackgroundToggledColorId(ui::ColorId background_toggled_color_id);

  // Sets the button's foreground color or toggled color with color ID when the
  // button wants to have a different foreground color from the default one.
  void SetForegroundColorId(ui::ColorId foreground_color_id);
  void SetForegroundToggledColorId(ui::ColorId foreground_toggled_color_id);

  // Sets the tile icon from an ImageSkia.
  void SetImage(gfx::ImageSkia image);

  // Sets the tooltip text of `icon_button_`.
  void SetIconButtonTooltipText(const std::u16string& text);

  // Sets the text of `label_`.
  void SetLabel(const std::u16string& label);

  // Returns the maximum width for `sub_label_`.
  int GetSubLabelMaxWidth() const;

  // Sets the text of the `sub_label_`.
  void SetSubLabel(const std::u16string& sub_label);

  // Sets visibility of `sub_label_`.
  void SetSubLabelVisibility(bool visible);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;

  bool is_icon_clickable() const { return is_icon_clickable_; }
  views::ImageButton* icon_button() { return icon_button_; }
  views::Label* label() { return label_; }
  views::Label* sub_label() { return sub_label_; }
  views::ImageView* drill_in_arrow() { return drill_in_arrow_; }

 private:
  friend class BluetoothFeaturePodControllerTest;
  friend class HotspotFeaturePodControllerTest;
  friend class NotificationCounterViewTest;

  // Returns the color id to use for the `icon_button_` and `drill_in_arrow_`
  // based on the tile's enabled and toggled state.
  ui::ColorId GetIconColorId() const;

  // Updates the ink drop hover color and ripple color for `icon_button_`.
  void UpdateIconButtonRippleColors();

  // Updates the focus ring color for `icon_button_` for better visibility.
  void UpdateIconButtonFocusRingColor();

  // Updates the color of `drill_in_arrow_` for better visibility.
  void UpdateDrillInArrowColor();

  // Updates `label_` attributes depending on whether a sub-label will be
  // visible.
  void SetCompactTileLabelPreferences(bool has_sub_label);

  // Ensures the ink drop is painted above the button's background.
  raw_ptr<views::InkDropContainerView, ExperimentalAsh> ink_drop_container_ =
      nullptr;

  // The vector icon for the tile, if one is set.
  raw_ptr<const gfx::VectorIcon, ExperimentalAsh> vector_icon_ = nullptr;

  // Customized value for the tile's background color and foreground color.
  absl::optional<ui::ColorId> background_color_;
  absl::optional<ui::ColorId> background_toggled_color_;
  absl::optional<ui::ColorId> foreground_color_;
  absl::optional<ui::ColorId> foreground_toggled_color_;

  // Owned by views hierarchy.
  raw_ptr<views::ImageButton, ExperimentalAsh> icon_button_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> label_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> sub_label_ = nullptr;
  raw_ptr<views::ImageView, ExperimentalAsh> drill_in_arrow_ = nullptr;

  // Whether the icon is separately clickable.
  bool is_icon_clickable_ = false;

  // Whether this button is togglable.
  bool is_togglable_ = false;

  // Whether the button is currently toggled.
  bool toggled_ = false;

  // The type of the feature tile that determines how it lays out its view.
  TileType type_;

  // Used to update tile colors and to set the drill-in button enabled state
  // when the button state changes.
  base::CallbackListSubscription enabled_changed_subscription_;

  base::WeakPtrFactory<FeatureTile> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_TILE_H_
