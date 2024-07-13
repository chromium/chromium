// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_TILE_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_TILE_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
class Insets;
}  // namespace gfx

namespace views {
class FlexLayoutView;
class ImageButton;
class ImageView;
class InkDropContainerView;
class Label;
}  // namespace views

namespace ash {

// The main button used in FeatureTilesContainerView, which acts as an entry
// point for features in QuickSettingsView.
//
// Note: Once http://b/298692153 is complete then this will be the type of tile
// used in the VC controls bubble as well.
//
// There are two TileTypes: Primary and Compact.
//
// The primary tile has an icon and title, and may have a subtitle. The icon may
// or may not be separately clickable. The tile has one of the following
// behaviors:
// 1. Launch surface        (e.g. Screen Capture)
// 2. Toggle                (e.g. Toggle Dark Theme)
// 3. Drill-in              (e.g. Go to Accessibility detailed view)
// 4. Toggle with drill-in  (e.g. Toggle Wi-Fi | Go to Network details)
// 5. Togglable tile with decorative drill-in (e.g. Selecting a VPN network)
//
// The compact tile has an icon and a single title, which may be
// multi-line. They are always placed in pairs side by side to take up the
// space of a regular FeatureTile. Regular tiles may switch to their compact
// version when necessary, e.g. when entering TabletMode. It presents one
// of the following behaviors:
// 1. Launch surface        (e.g. Screen Capture)
// 2. Toggle                (e.g. Toggle Auto-rotate)
// 3. Drill-in              (e.g. Go to Cast detailed view)
//
// Support for download UI is in the process of being added. This will allow a
// compact tile to indicate the progress of a download it is associated with.
// The initial use-case will be DLC downloading for the "Live caption" feature
// tile in the VC controls bubble, though the API for setting download state
// should be general enough that it can be used for anything download-related.
// See http://b/298692153 for details.
class ASH_EXPORT FeatureTile : public views::Button {
  METADATA_HEADER(FeatureTile, views::Button)

 public:
  // Used in the FeatureTile constructor to set the tile view type.
  enum class TileType {
    kPrimary = 0,
    kCompact = 1,
    kMaxValue = kCompact,
  };

  // The possible states the download progress UI can be in. The download
  // progress UI is currently only supported for compact tiles.
  //
  // TODO(b/315188874): Add full support for all download states.
  enum class DownloadState {
    kNone,         // The default state, e.g. this tile is not associated
                   // with a download. If this tile is of type
                   // `TileType::kPrimary` then it should always be in this
                   // download state.
    kPending,      // The download has not yet started. The tile's label is
                   // changed to "Download pending". The tile is not
                   // interactable while in this state.
    kDownloading,  // The download is in progress. The tile's label is changed
                   // to "Downloading X%" and a download progress indicator is
                   // made visible. The tile is not interactable while in this
                   // state.
    kDownloaded,   // The download finished successfully.
    kError,        // The download finished with an error. The tile is not
                   // interactable while in this state.
  };

  // Constructor for FeatureTiles. `callback` will be called when interacting
  // with the main part of the button, which accounts for the whole tile.
  // If the icon is not separately clickable (the default), `callback` will
  // also be called when clicking on the icon.
  explicit FeatureTile(PressedCallback callback,
                       bool is_togglable = true,
                       TileType type = TileType::kPrimary);
  FeatureTile(const FeatureTile&) = delete;
  FeatureTile& operator=(const FeatureTile&) = delete;
  ~FeatureTile() override;

  // Implement this class to get notified of certain state changes to this tile.
  // Currently this only exists for testing purposes.
  class Observer : public base::CheckedObserver {
   public:
    // Called when this tile's download state changes. `download_state` is the
    // new download state, and `progress` is the new download progress
    // (currently only meaningful when the download state is
    // `DownloadState::kDownloading`).
    virtual void OnDownloadStateChanged(DownloadState download_state,
                                        int progress) = 0;
  };

  // Adds/removes an observer of this tile.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets whether the icon on the left is clickable, separate from clicking on
  // the tile itself. Use SetIconClickCallback() to set the callback. This
  // function is separate from SetIconClickCallback() because it's likely that
  // FeatureTile users will want to set the callback once but may want to switch
  // the icon between being clickable or not (e.g. the network icon based on
  // Ethernet vs. Wi-Fi).
  void SetIconClickable(bool clickable);

  // Sets the `callback` for clicks on `icon_button_`.
  void SetIconClickCallback(base::RepeatingCallback<void()> callback);

  // Sets the `on_title_container_bounds_changed_` callback.
  void SetOnTitleBoundsChangedCallback(
      base::RepeatingCallback<void()> callback);

  // Creates a decorative `drill_in_arrow_` on the right side of the tile. This
  // indicates to the user that the tile shows a detailed view when pressed.
  void CreateDecorativeDrillInArrow();

  // Updates the colors of the background and elements of the button.
  void UpdateColors();

  // Updates the `toggled_` state of the tile. If the tile is not togglable,
  // `toggled_` will always be false.
  void SetToggled(bool toggled);
  bool IsToggled() const;

  // Sets the vector icon.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Sets margins for 'title_container_' in the tile.
  void SetTitleContainerMargins(const gfx::Insets& insets);

  // Setters to apply custom background colors.
  void SetBackgroundColorId(ui::ColorId background_color_id);
  void SetBackgroundToggledColorId(ui::ColorId background_toggled_color_id);
  void SetBackgroundDisabledColorId(ui::ColorId background_disabled_color_id);

  // Sets the radius determining the tile's curved edges.
  void SetButtonCornerRadius(const int radius);

  // Setters to apply custom foreground colors.
  void SetForegroundColorId(ui::ColorId foreground_color_id);
  void SetForegroundToggledColorId(ui::ColorId foreground_toggled_color_id);
  void SetForegroundDisabledColorId(ui::ColorId foreground_disabled_color_id);
  void SetForegroundOptionalColorId(ui::ColorId foreground_optional_color_id);
  void SetForegroundOptionalToggledColorId(
      ui::ColorId foreground_optional_toggled_color_id);

  // Sets a custom color for the tile's ink drop, when its toggled.
  void SetInkDropToggledBaseColorId(ui::ColorId ink_drop_toggled_base_color_id);

  // Sets the tile icon from an ImageSkia.
  void SetImage(gfx::ImageSkia image);

  // Sets the tooltip text of `icon_button_`.
  void SetIconButtonTooltipText(const std::u16string& text);

  // Sets the text of `label_`. If `VcDlcUi` is enabled and there is an on-going
  // download associated with this tile then the new label won't be reflected in
  // the UI until the download finishes. Also note that download-related labels
  // (like "Downloading 7%" or "Download pending") should not be specified using
  // this method - those labels are automatically set when the download state
  // changes.
  void SetLabel(const std::u16string& label);

  // Returns the maximum width for `sub_label_`.
  int GetSubLabelMaxWidth() const;

  // Sets the text of the `sub_label_`.
  void SetSubLabel(const std::u16string& sub_label);

  // Sets visibility of `sub_label_`.
  void SetSubLabelVisibility(bool visible);

  // Sets the state of this tile's download progress UI. See the documentation
  // for the `DownloadState` enum for more details on how a particular download
  // state affects the tile. `progress` is an integer in the range [0, 100], and
  // is ignored when `state` is not `DownloadState::kDownloading`.
  void SetDownloadState(DownloadState state, int progress);

  // views::View:
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  base::WeakPtr<FeatureTile> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  TileType tile_type() { return type_; }
  bool is_icon_clickable() const { return is_icon_clickable_; }
  views::ImageButton* icon_button() { return icon_button_; }
  views::Label* label() { return label_; }
  views::Label* sub_label() { return sub_label_; }
  views::ImageView* drill_in_arrow() { return drill_in_arrow_; }
  int corner_radius() const { return corner_radius_; }

  DownloadState download_state_for_testing() const { return download_state_; }
  int download_progress_for_testing() const {
    return download_progress_percent_;
  }

 private:
  friend class BluetoothFeaturePodControllerTest;
  friend class HotspotFeaturePodControllerTest;
  friend class NotificationCounterViewTest;

  // A `views::Background` that visually indicates download progress.
  // Automatically handles both LTR and RTL layouts.
  class ProgressBackground : public views::Background {
   public:
    ProgressBackground(ui::ColorId progress_color_id,
                       ui::ColorId background_color_id);
    ProgressBackground(const ProgressBackground&) = delete;
    ProgressBackground& operator=(const ProgressBackground&) = delete;
    ~ProgressBackground() override = default;

    // views::Background:
    void Paint(gfx::Canvas* canvas, views::View* view) const override;

   private:
    // The `ui::ColorId`s for both the progress- and non-progress-(i.e.
    // "background-") portions of the background.
    const ui::ColorId progress_color_id_;
    const ui::ColorId background_color_id_;
  };

  // views::Button:
  void OnSetTooltipText(const std::u16string& tooltip_text) override;

  // Creates child views of Feature Tile. The constructed view will vary
  // depending on the button's `type_`.
  void CreateChildViews();

  // Returns the color id to use for the `icon_button_` and `drill_in_arrow_`
  // based on the tile's enabled and toggled state.
  ui::ColorId GetIconColorId() const;

  // Updates the ink drop hover color and ripple color for `icon_button_`.
  void UpdateIconButtonRippleColors();

  // Updates the focus ring color for `icon_button_` for better visibility.
  void UpdateIconButtonFocusRingColor();

  // Updates the color of `drill_in_arrow_` for better visibility.
  void UpdateDrillInArrowColor();

  // Updates the accessibility properties directly in the cache, like the role
  // and the toggle state.
  void UpdateAccessibilityProperties();

  // Updates `label_` attributes depending on whether a sub-label will be
  // visible.
  void SetCompactTileLabelPreferences(bool has_sub_label);

  // Sets the tile's label to its download-related version (e.g. "Downloading
  // 7%" or "Download pending"). This is different from `SetLabel()` because
  // `SetLabel()` is intended to be used externally for setting the tile's
  // non-download-related label (i.e. the "client-specified" label), whereas
  // this method is only used internally by this class to temporarily switch to
  // a different label during download. An optional tooltip can be included if
  // the tooltip should differ from the `download_label`.
  void SetDownloadLabel(const std::u16string& download_label,
                        std::optional<std::u16string> tooltip = std::nullopt);

  // Updates the tile's label according to the current download state. Note that
  // this method assumes the download-related state (e.g. `download_state_` and
  // `download_progress_percent_`) is current, so it is up to the client to
  // perform any download-related state changes prior to calling this.
  void UpdateLabelForDownloadState();

  // Notifies all observers of this tile's current download state. Should only
  // be called when the download state actually changes.
  void NotifyDownloadStateChanged();

  // A list of this tile's observers.
  base::ObserverList<Observer> observers_;

  // Ensures the ink drop is painted above the button's background.
  raw_ptr<views::InkDropContainerView> ink_drop_container_ = nullptr;

  // The vector icon for the tile, if one is set.
  raw_ptr<const gfx::VectorIcon> vector_icon_ = nullptr;

  // Customized value for the tile's background color and foreground color.
  std::optional<ui::ColorId> background_color_;
  std::optional<ui::ColorId> background_toggled_color_;
  std::optional<ui::ColorId> background_disabled_color_;
  std::optional<ui::ColorId> foreground_color_;
  std::optional<ui::ColorId> foreground_toggled_color_;
  std::optional<ui::ColorId> foreground_optional_color_;
  std::optional<ui::ColorId> foreground_optional_toggled_color_;
  std::optional<ui::ColorId> foreground_disabled_color_;

  // Customized value for the tile's ink drop color.
  std::optional<ui::ColorId> ink_drop_toggled_base_color_;

  // Owned by views hierarchy.
  raw_ptr<views::ImageButton> icon_button_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::Label> sub_label_ = nullptr;
  raw_ptr<views::ImageView> drill_in_arrow_ = nullptr;
  raw_ptr<views::FlexLayoutView> title_container_ = nullptr;

  // The radius of the tile's curved edges.
  int corner_radius_;

  // Whether the icon is separately clickable.
  bool is_icon_clickable_ = false;

  // Whether this button is togglable.
  const bool is_togglable_ = false;

  // Whether the button is currently toggled.
  bool toggled_ = false;

  // The non-download-related (a.k.a. "client-specified") text of this tile's
  // label/tooltip. The tile's label/tooltip may change when its downloading
  // state changes, so this is used to store the original, client-specified
  // label/tooltip for later reference (e.g. when a download finishes and the
  // tile needs to show the original label again).
  std::u16string client_specified_label_text_;
  std::u16string client_specified_tooltip_text_;

  // The type of the feature tile that determines how it lays out its view.
  TileType type_;

  // Used to update tile colors and to set the drill-in button enabled state
  // when the button state changes.
  base::CallbackListSubscription enabled_changed_subscription_;

  // The download state this tile is in. A tile is not associated with a
  // download by default.
  DownloadState download_state_ = DownloadState::kNone;

  // True when `UpdateLabelForDownloadState()` is happening.
  bool updating_download_state_labels_ = false;

  // The download progress, as an integer percentage in the range [0, 100]. Only
  // has meaning when the tile is in an active download state.
  int download_progress_percent_ = 0;

  // Runs when `title_container_`'s bounds is changed.
  base::RepeatingClosure on_title_container_bounds_changed_;

  base::WeakPtrFactory<FeatureTile> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_TILE_H_
