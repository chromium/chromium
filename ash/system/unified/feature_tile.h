// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_TILE_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_TILE_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

// The main button used in FeatureTilesContainerView, which acts as an entry
// point for features in QuickSettingsView.

// There are two TileTypes: Primary and Compact.

// The primary tile has an icon and title, and may have a subtitle and a
// drill-in button. It presents one of the following behaviors:
// 1. Launch surface        (e.g. Screen Capture)
// 2. Toggle                (e.g. Toggle Dark Theme)
// 3. Drill-in              (e.g. Go to Accessibility detailed view)
// 4. Toggle with drill-in  (e.g. Toggle Wi-Fi | go to Network detailed view)

// The compact tile has an icon and a single title, which may be
// multi-line. They are always placed in pairs side by side to take up the
// space of a regular FeatureTile. Regular tiles may switch to their compact
// version when necessary, e.g. when entering TabletMode. It presents one
// of the following behaviors:
// 1. Launch surface        (e.g. Screen Capture)
// 2. Toggle                (e.g. Toggle Auto-rotate)
class ASH_EXPORT FeatureTile : public views::Button {
 public:
  METADATA_HEADER(FeatureTile);

  // Used in the FeatureTile constructor to set the tile view type.
  enum class TileType {
    kPrimary = 0,
    kCompact = 1,
    kMaxValue = kCompact,
  };

  // Constructor for prototype tiles without a callback. Applies placeholder
  // icons and titles.
  // TODO(b/252871301): Remove when having implemented each feature tile.
  explicit FeatureTile(TileType type = TileType::kPrimary);

  // Constructor for FeatureTiles. `callback` will be called when interacting
  // with the main part of the button, which accounts for the whole tile.
  // For primary tiles with drill-in, `callback` is called when interacting with
  // the left side of the button, since the right side holds the drill-in
  // button.
  explicit FeatureTile(base::RepeatingCallback<void()> callback,
                       bool is_togglable = true,
                       TileType type = TileType::kPrimary);
  FeatureTile(const FeatureTile&) = delete;
  FeatureTile& operator=(const FeatureTile&) = delete;
  ~FeatureTile() override;

  // Creates child views of Feature Tile. The constructed view will vary
  // depending on the button's `type_`.
  void CreateChildViews();

  // Creates the `drill_container_` which holds the `drill_in_button_`.
  // `callback` is called when interacting with the right side of the button
  // where the drill-in button exists.
  void CreateDrillInButton(base::RepeatingCallback<void()> callback,
                           const std::u16string& tooltip_text);

  TileType tile_type() { return type_; }

  // Updates the colors of the background and elements of the button.
  void UpdateColors();

  // Updates the `toggled_` state of the button. If button is not togglable,
  // `toggled_` will always be false.
  void SetToggled(bool toggled);
  bool IsToggled() const;

  // Sets the vector icon.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Sets the tile icon from an ImageSkia.
  void SetImage(gfx::ImageSkia image);

  // Sets the text of `label_`.
  void SetLabel(const std::u16string& label);

  // Sets the text of the `sub_label_`.
  void SetSubLabel(const std::u16string& sub_label);

  // Sets visibility of `sub_label_`.
  void SetSubLabelVisibility(bool visible);

  // Sets the tooltip text of `drill_in_button_`.
  void SetDrillInButtonTooltipText(const std::u16string& text);

  views::ImageView* icon() { return icon_; }
  views::Label* label() { return label_; }
  views::Label* sub_label() { return sub_label_; }
  views::LabelButton* drill_in_button() { return drill_in_button_; }

 private:
  friend class BluetoothFeaturePodControllerTest;
  friend class NotificationCounterViewTest;

  // The vector icon for the tile, if one is set.
  const gfx::VectorIcon* vector_icon_ = nullptr;

  // Owned by views hierarchy.
  views::ImageView* icon_ = nullptr;
  views::Label* label_ = nullptr;
  views::Label* sub_label_ = nullptr;
  views::LabelButton* drill_in_button_ = nullptr;

  // Whether this button is togglable.
  bool is_togglable_ = false;

  // Whether the button is currently toggled.
  bool toggled_ = false;

  // The type of the feature tile that determines how it lays out its view.
  TileType type_;

  // Used to set the drill-in button enabled state when the button state
  // changes.
  base::CallbackListSubscription enabled_changed_subscription_;

  base::WeakPtrFactory<FeatureTile> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_TILE_H_
