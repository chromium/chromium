// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_main_menu_view.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/game_dashboard/game_dashboard_battery_view.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_metrics.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/arc_compat_mode_util.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/style/switch.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/power/power_status.h"
#include "ash/system/time/time_view.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/unified/feature_pod_button.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/frame_header.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Corner radius for the main menu.
constexpr int kBubbleCornerRadius = 24;
// Horizontal padding for the border around the main menu.
constexpr int kPaddingWidth = 20;
// Vertical padding for the border around the main menu.
constexpr int kPaddingHeight = 20;
// Padding between children in a row or column.
constexpr int kCenterPadding = 8;
// Main Menu fixed width.
constexpr int kMainMenuFixedWidth = 416;
// Corner radius for the detail row container.
constexpr float kDetailRowCornerRadius = 16.0f;
// Corner radius for feature tiles.
constexpr int kTileCornerRadius = 20;
// Line height for feature tiles with sub-labels
constexpr int kTileLabelLineHeight = 16;
// Feature Tile default padding when there are 3 Feature Tiles in the
// Shortcut Tiles row. Also used as the default padding when creating
// a Feature Tile.
constexpr gfx::Insets kThreeTilePadding = gfx::Insets::TLBR(0, 24, 10, 24);
// Feature tile padding when there are 2 Feature Tiles in the Shortcut Tiles
// row.
constexpr gfx::Insets kTwoTilePadding = gfx::Insets::TLBR(10, 12, 10, 24);
// Feature Tile padding when there are 4 Feature Tiles in the Shortcut Tiles
// row.
constexpr gfx::Insets kFourTilePadding = gfx::Insets::TLBR(0, 10, 10, 10);
// Feature Tile Icon Padding.
constexpr gfx::Insets kCompactTileIconPadding = gfx::Insets::TLBR(12, 8, 4, 8);
// Primary Feature Tile Icon Padding.
constexpr gfx::Insets kPrimaryTileIconPadding = gfx::Insets::TLBR(8, 20, 8, 8);
// Primary Feature Tile Label Padding.
constexpr gfx::Insets kPrimaryTileLabelPadding = gfx::Insets::TLBR(0, 0, 0, 15);
// Clock View Padding.
constexpr gfx::Insets kClockViewPadding = gfx::Insets::VH(10, 0);

// Row corners used for the top row of a multi-feature row collection.
constexpr gfx::RoundedCornersF kTopMultiRowCorners =
    gfx::RoundedCornersF(/*upper_left=*/kDetailRowCornerRadius,
                         /*upper_right=*/kDetailRowCornerRadius,
                         /*lower_right=*/2.0f,
                         /*lower_left=*/2.0f);
// Row corners used for the bottom row of a multi-featue row collection.
constexpr gfx::RoundedCornersF kBottomMultiRowCorners =
    gfx::RoundedCornersF(/*upper_left=*/2.0f,
                         /*upper_right=*/2.0f,
                         /*lower_right=*/kDetailRowCornerRadius,
                         /*lower_left=*/kDetailRowCornerRadius);
// Row corners used for a single feature row collection.
constexpr gfx::RoundedCornersF kSingleRowCorners =
    gfx::RoundedCornersF(kDetailRowCornerRadius);

// For setup button pulse animation.
constexpr int kSetupPulseExtraHalfSize = 32;
constexpr int kSetupPulseTimes = 3;
constexpr base::TimeDelta kSetupPulseDuration = base::Seconds(2);

constexpr char kSetupNudgeId[] = "SetupNudgeId";
constexpr char kHelpUrl[] =
    "https://support.google.com/chromebook/?p=game-dashboard-help";

// Creates an individual Game Dashboard Tile.
std::unique_ptr<FeatureTile> CreateFeatureTile(
    base::RepeatingClosure callback,
    bool is_togglable,
    FeatureTile::TileType type,
    int id,
    const gfx::VectorIcon& icon,
    const std::u16string& text,
    const std::optional<std::u16string>& sub_label) {
  auto tile =
      std::make_unique<FeatureTile>(std::move(callback), is_togglable, type);

  views::Label* tile_sub_label = tile->sub_label();
  if (sub_label) {
    tile->SetSubLabel(sub_label.value());
    tile->SetSubLabelVisibility(true);
    tile_sub_label->SetLineHeight(kTileLabelLineHeight);
  }

  tile->SetID(id);
  tile->SetVectorIcon(icon);
  tile->SetLabel(text);
  tile->SetTooltipText(text);
  tile->SetButtonCornerRadius(kTileCornerRadius);
  tile->SetTitleContainerMargins(kThreeTilePadding);

  // Default state colors.
  tile->SetBackgroundColorId(cros_tokens::kCrosSysSystemOnBase);
  tile->SetForegroundColorId(cros_tokens::kCrosSysOnSurface);
  tile->SetForegroundOptionalColorId(cros_tokens::kCrosSysOnSurface);

  // Toggled state colors.
  tile->SetBackgroundToggledColorId(
      cros_tokens::kCrosSysSystemPrimaryContainer);
  tile->SetForegroundToggledColorId(
      cros_tokens::kCrosSysSystemOnPrimaryContainer);
  tile->SetForegroundOptionalToggledColorId(
      cros_tokens::kCrosSysSystemOnPrimaryContainer);

  // Disabled state colors.
  tile->SetBackgroundDisabledColorId(cros_tokens::kCrosSysSystemOnBaseOpaque);

  views::Label* tile_label = tile->label();

  tile_label->SetLineHeight(kTileLabelLineHeight);

  // Readjust Compact Tiles.
  views::ImageButton* tile_icon = tile->icon_button();
  if (type == FeatureTile::TileType::kCompact) {
    // Adjust internal spacing.
    tile_icon->SetProperty(views::kMarginsKey, kCompactTileIconPadding);

    // Adjust line and text specifications.
    tile_label->SetFontList(
        TypographyProvider::Get()
            ->ResolveTypographyToken(TypographyToken::kCrosAnnotation2)
            .DeriveWithSizeDelta(1)
            .DeriveWithHeightUpperBound(16));
    tile_sub_label->SetFontList(
        TypographyProvider::Get()->ResolveTypographyToken(
            TypographyToken::kCrosAnnotation2));

  } else {
    // Resize the icon and its margins.
    tile_icon->SetPreferredSize(
        gfx::Size(20, tile_icon->GetPreferredSize().height()));
    tile_icon->SetProperty(views::kMarginsKey, kPrimaryTileIconPadding);

    // Adjust line specifications and enable text wrapping.
    tile_label->SetProperty(views::kMarginsKey, kPrimaryTileLabelPadding);
    tile_label->SetMultiLine(true);
  }

  // Setup focus ring.
  views::FocusRing::Get(tile.get())->SetColorId(cros_tokens::kCrosSysPrimary);
  return tile;
}

std::unique_ptr<FeaturePodIconButton> CreateIconButton(
    base::RepeatingClosure callback,
    int id,
    const gfx::VectorIcon& icon,
    const std::u16string& text) {
  auto icon_button = std::make_unique<FeaturePodIconButton>(
      std::move(callback), /*is_togglable=*/false);
  icon_button->SetID(id);
  icon_button->SetVectorIcon(icon);
  icon_button->SetTooltipText(text);
  return icon_button;
}

bool IsGameControlsFeatureEnabled(ArcGameControlsFlag flags) {
  return game_dashboard_utils::IsFlagSet(flags, ArcGameControlsFlag::kEnabled);
}

// Helper function to configure the feature row button designs and return the
// layout manager.
views::BoxLayout* ConfigureFeatureRowLayout(views::Button* button,
                                            const gfx::RoundedCornersF& corners,
                                            bool enabled) {
  auto* layout = button->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      /*inside_border_insets=*/gfx::Insets::VH(16, 16)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  button->SetNotifyEnterExitOnChild(true);
  button->SetEnabled(enabled);
  button->SetBackground(views::CreateThemedRoundedRectBackground(
      enabled ? cros_tokens::kCrosSysSystemOnBase
              : cros_tokens::kCrosSysSystemOnBaseOpaque,
      corners));

  // Set up highlight ink drop and focus ring.
  views::HighlightPathGenerator::Install(
      button, std::make_unique<views::RoundRectHighlightPathGenerator>(
                  gfx::Insets(), corners));

  // Set up press ripple.
  auto* ink_drop = views::InkDrop::Get(button);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop->GetInkDrop()->SetShowHighlightOnHover(false);
  ink_drop->GetInkDrop()->SetShowHighlightOnFocus(false);
  ink_drop->SetVisibleOpacity(1.0f);
  ink_drop->SetBaseColorId(cros_tokens::kCrosSysRippleNeutralOnSubtle);

  // Set up focus ring.
  auto* focus_ring = views::FocusRing::Get(button);
  focus_ring->SetHaloInset(-5);
  focus_ring->SetHaloThickness(2);
  focus_ring->SetColorId(cros_tokens::kCrosSysPrimary);

  return layout;
}

// -----------------------------------------------------------------------------
// FeatureHeader:

// `FeatureHeader` includes icon, title and sub-title.
// +---------------------+
// | |icon|  |title|     |
// |         |sub-title| |
// +---------------------+
class FeatureHeader : public views::View {
  METADATA_HEADER(FeatureHeader, views::View)

 public:
  FeatureHeader(bool is_enabled,
                const gfx::VectorIcon& icon,
                const std::u16string& title)
      : vector_icon_(&icon) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    // Add icon.
    icon_view_ = AddChildView(std::make_unique<views::ImageView>());
    icon_view_->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase,
        /*radius=*/12.0f));
    icon_view_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(6, 6)));
    icon_view_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 16));

    // Add title and sub-title.
    auto* tag_container =
        AddChildView(std::make_unique<views::BoxLayoutView>());
    tag_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
    tag_container->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    // Flex `tag_container` to fill empty space.
    layout->SetFlexForView(tag_container, /*flex=*/1);

    // Add title.
    title_ = tag_container->AddChildView(std::make_unique<views::Label>(title));
    title_->SetAutoColorReadabilityEnabled(false);
    title_->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosTitle2));
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_->SetMultiLine(true);
    // Add sub-title.
    sub_title_ = tag_container->AddChildView(std::make_unique<views::Label>());
    sub_title_->SetAutoColorReadabilityEnabled(false);
    sub_title_->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosAnnotation2));
    sub_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    sub_title_->SetMultiLine(true);

    UpdateColors(is_enabled);
  }

  FeatureHeader(const FeatureHeader&) = delete;
  FeatureHeader& operator=(const FeatureHeader) = delete;
  ~FeatureHeader() override = default;

  const views::Label* GetSubtitle() { return sub_title_.get(); }

  void UpdateColors(bool is_enabled) {
    const auto color_id = is_enabled ? cros_tokens::kCrosSysOnSurface
                                     : cros_tokens::kCrosSysDisabled;
    icon_view_->SetImage(ui::ImageModel::FromVectorIcon(*vector_icon_, color_id,
                                                        /*icon_size=*/20));
    title_->SetEnabledColorId(color_id);
    sub_title_->SetEnabledColorId(is_enabled
                                      ? cros_tokens::kCrosSysOnSurfaceVariant
                                      : cros_tokens::kCrosSysDisabled);
  }

  void UpdateSubtitle(const std::u16string& text) {
    // For multiline label, if the fixed width is not set, the preferred size is
    // re-calcuated based on previous label size as available size instead of
    // its real available size when changing the text. For `sub_title_`, it
    // takes the whole width of its parent's width as fixed width after layout.
    if (!sub_title_->GetFixedWidth()) {
      if (int width = sub_title_->parent()->size().width(); width != 0) {
        sub_title_->SizeToFit(width);
      }
    }
    sub_title_->SetText(text);
  }

 private:
  const raw_ptr<const gfx::VectorIcon> vector_icon_;

  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> sub_title_ = nullptr;
};

BEGIN_METADATA(FeatureHeader)
END_METADATA

}  // namespace

// -----------------------------------------------------------------------------
// ScreenSizeRow:

// ScreenSizeRow includes `FeatureHeader` and right arrow icon.
// +------------------------------------------------+
// | |feature header|                           |>| |
// +------------------------------------------------+
class GameDashboardMainMenuView::ScreenSizeRow : public views::Button {
  METADATA_HEADER(ScreenSizeRow, views::Button)

 public:
  ScreenSizeRow(GameDashboardMainMenuView* main_menu,
                PressedCallback callback,
                ResizeCompatMode resize_mode,
                ArcResizeLockType resize_lock_type)
      : views::Button(std::move(callback)) {
    SetID(VIEW_ID_GD_SCREEN_SIZE_TILE);

    bool enabled = false;
    int tooltip = 0;
    std::u16string subtitle;
    switch (resize_lock_type) {
      case ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE:
      case ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE:
        enabled = true;
        subtitle = compat_mode_util::GetText(resize_mode);
        break;
      case ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE:
        enabled = false;
        tooltip =
            IDS_ASH_ARC_APP_COMPAT_DISABLED_COMPAT_MODE_BUTTON_TOOLTIP_PHONE;
        DCHECK_NE(resize_mode, ResizeCompatMode::kResizable)
            << "The resize mode should never be resizable with an "
               "ArcResizeLockType of RESIZE_DISABLED_NONTOGGLABLE.";
        if (resize_mode == ResizeCompatMode::kPhone) {
          subtitle = l10n_util::GetStringUTF16(
              IDS_ASH_GAME_DASHBOARD_SCREEN_SIZE_ONLY_PORTRAIT);
        } else {
          subtitle = l10n_util::GetStringUTF16(
              IDS_ASH_GAME_DASHBOARD_SCREEN_SIZE_ONLY_LANDSCAPE);
        }
        break;
      case ArcResizeLockType::NONE:
        enabled = false;
        tooltip = IDS_ASH_GAME_DASHBOARD_FEATURE_NOT_AVAILABLE_TOOLTIP;

        // Set the subtitle text based on whether the size button in the frame
        // header is present.
        auto* frame_header =
            chromeos::FrameHeader::Get(views::Widget::GetWidgetForNativeWindow(
                main_menu->context_->game_window()));
        views::FrameCaptionButton* size_button =
            frame_header->caption_button_container()->size_button();
        if (size_button && size_button->GetVisible()) {
          subtitle = l10n_util::GetStringUTF16(
              IDS_ASH_GAME_DASHBOARD_SCREEN_SIZE_EXIT_FULLSCREEN);
        } else {
          subtitle = l10n_util::GetStringUTF16(
              IDS_ASH_GAME_DASHBOARD_SCREEN_SIZE_ONLY_FULLSCREEN);
        }
        break;
    }

    const std::u16string title = l10n_util::GetStringUTF16(
        IDS_ASH_GAME_DASHBOARD_SCREEN_SIZE_SETTINGS_TITLE);
    SetTooltipText(tooltip ? l10n_util::GetStringUTF16(tooltip) : title);
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_ASH_GAME_DASHBOARD_SCREEN_SIZE_SETTINGS_BUTTON_A11Y_LABEL));

    auto* layout =
        ConfigureFeatureRowLayout(this, kBottomMultiRowCorners, enabled);
    // Add header.
    feature_header_ = AddChildView(std::make_unique<FeatureHeader>(
        enabled, compat_mode_util::GetIcon(resize_mode), title));
    layout->SetFlexForView(feature_header_, /*flex=*/1);
    feature_header_->UpdateSubtitle(subtitle);
    // Add arrow icon.
    AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            kQuickSettingsRightArrowIcon,
            enabled ? cros_tokens::kCrosSysOnSurface
                    : cros_tokens::kCrosSysDisabled)));
  }

  ScreenSizeRow(const ScreenSizeRow&) = delete;
  ScreenSizeRow& operator=(const ScreenSizeRow) = delete;
  ~ScreenSizeRow() override = default;

  FeatureHeader* feature_header() { return feature_header_; }

 private:
  raw_ptr<FeatureHeader> feature_header_;
};

BEGIN_METADATA(GameDashboardMainMenuView, ScreenSizeRow)
END_METADATA

// -----------------------------------------------------------------------------
// GameDashboardMainMenuView::GameControlsDetailsRow:

// `GameControlsDetailsRow` includes `FeatureHeader`, set up button or switch
// button with drill in arrow icon. If there is no Game Controls set up, it
// shows as:
// +------------------------------------------------+
// | |feature header|                |set_up button||
// +------------------------------------------------+
// Otherwise, it shows as:
// +------------------------------------------------+
// | |feature header|      |switch| |drill in arrow||
// +------------------------------------------------+
class GameDashboardMainMenuView::GameControlsDetailsRow : public views::Button {
  METADATA_HEADER(GameControlsDetailsRow, views::Button)

 public:
  explicit GameControlsDetailsRow(GameDashboardMainMenuView* main_menu,
                                  const gfx::RoundedCornersF& row_corners)
      : views::Button(
            base::BindRepeating(&GameControlsDetailsRow::OnButtonPressed,
                                base::Unretained(this))),
        main_menu_(main_menu) {
    CacheAppName();
    SetID(VIEW_ID_GD_CONTROLS_DETAILS_ROW);

    const auto flags =
        game_dashboard_utils::GetGameControlsFlag(GetGameWindow());
    CHECK(flags);

    SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_GAME_DASHBOARD_GC_CONTROLS_DETAILS_BUTTON_TOOLTIP));

    const bool is_available = game_dashboard_utils::IsFlagSet(
        *flags, ArcGameControlsFlag::kAvailable);
    auto* layout = ConfigureFeatureRowLayout(this, row_corners, is_available);

    // Add header.
    header_ = AddChildView(std::make_unique<FeatureHeader>(
        /*is_enabled=*/is_available, kGdGameControlsIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_GAME_DASHBOARD_CONTROLS_TILE_BUTTON_TITLE)));
    // Flex `header_` to fill the empty space.
    layout->SetFlexForView(header_, /*flex=*/1);

    // Add setup button, or feature switch and drill-in arrow.
    if (!is_available ||
        game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEmpty)) {
      // Add setup button.
      header_->UpdateSubtitle(l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_GC_SET_UP_SUB_TITLE));
      setup_button_ = AddChildView(std::make_unique<PillButton>(
          base::BindRepeating(&GameControlsDetailsRow::OnSetUpButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_ASH_GAME_DASHBOARD_GC_SET_UP_BUTTON_LABEL),
          PillButton::Type::kPrimaryWithoutIcon,
          /*icon=*/nullptr));
      setup_button_->SetProperty(views::kMarginsKey,
                                 gfx::Insets::TLBR(0, 20, 0, 0));
      setup_button_->SetEnabled(is_available);
      if (!is_available) {
        setup_button_->SetTooltipText(l10n_util::GetStringUTF16(
            IDS_ASH_GAME_DASHBOARD_FEATURE_NOT_AVAILABLE_TOOLTIP));
      }
    } else {
      // Add switch_button to enable or disable game controls.
      feature_switch_ =
          AddChildView(std::make_unique<Switch>(base::BindRepeating(
              &GameControlsDetailsRow::OnFeatureSwitchButtonPressed,
              base::Unretained(this))));
      feature_switch_->SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(0, 8, 0, 18));
      const bool is_feature_enabled = IsGameControlsFeatureEnabled(*flags);
      feature_switch_->SetIsOn(is_feature_enabled);
      feature_switch_->SetTooltipText(l10n_util::GetStringUTF16(
          feature_switch_->GetIsOn()
              ? IDS_ASH_GAME_DASHBOARD_GC_FEATURE_SWITCH_TOOLTIPS_OFF
              : IDS_ASH_GAME_DASHBOARD_GC_FEATURE_SWITCH_TOOLTIPS_ON));
      // Add arrow icon.
      arrow_icon_ = AddChildView(std::make_unique<views::ImageView>());

      UpdateColors(is_feature_enabled);
      SetFocusBehavior(is_feature_enabled ? FocusBehavior::ALWAYS
                                          : FocusBehavior::ACCESSIBLE_ONLY);

      UpdateSubtitle(/*is_game_controls_enabled=*/is_feature_enabled);
    }
  }

  GameControlsDetailsRow(const GameControlsDetailsRow&) = delete;
  GameControlsDetailsRow& operator=(const GameControlsDetailsRow) = delete;
  ~GameControlsDetailsRow() override = default;

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override {
    if (is_visible) {
      MaybeDecorateSetupButton();
    } else {
      RemoveSetupButtonDecorationIfAny();
    }
  }

  PillButton* setup_button() { return setup_button_; }
  Switch* feature_switch() { return feature_switch_; }

 private:
  void OnButtonPressed() {
    const auto flags =
        game_dashboard_utils::GetGameControlsFlag(GetGameWindow());
    DCHECK(flags && game_dashboard_utils::IsFlagSet(
                        *flags, ArcGameControlsFlag::kAvailable));

    // Do nothing if Game Controls is disabled.
    if (!IsGameControlsFeatureEnabled(*flags)) {
      return;
    }

    EnableEditMode();
  }

  void OnSetUpButtonPressed() { EnableEditMode(); }

  void OnFeatureSwitchButtonPressed() {
    const bool is_switch_on = feature_switch_->GetIsOn();
    // When `feature_switch_` toggles on or off, it updates the colors but does
    // not enable or disable this button.
    UpdateColors(is_switch_on);
    SetFocusBehavior(is_switch_on ? FocusBehavior::ALWAYS
                                  : FocusBehavior::ACCESSIBLE_ONLY);

    auto* game_window = GetGameWindow();
    game_window->SetProperty(
        kArcGameControlsFlagsKey,
        game_dashboard_utils::UpdateFlag(
            game_window->GetProperty(kArcGameControlsFlagsKey),
            static_cast<ArcGameControlsFlag>(
                /*enable_flag=*/ArcGameControlsFlag::kEnabled |
                ArcGameControlsFlag::kHint),
            is_switch_on));
    feature_switch_->SetTooltipText(l10n_util::GetStringUTF16(
        is_switch_on ? IDS_ASH_GAME_DASHBOARD_GC_FEATURE_SWITCH_TOOLTIPS_OFF
                     : IDS_ASH_GAME_DASHBOARD_GC_FEATURE_SWITCH_TOOLTIPS_ON));

    main_menu_->UpdateGameControlsTile();
    UpdateSubtitle(/*is_game_controls_enabled=*/is_switch_on);

    RecordGameDashboardControlsFeatureToggleState(
        main_menu_->context_->app_id(), is_switch_on);
  }

  void UpdateColors(bool enabled) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        enabled ? cros_tokens::kCrosSysSystemOnBase
                : cros_tokens::kCrosSysSystemOnBaseOpaque,
        kTopMultiRowCorners));
    header_->UpdateColors(enabled);
    CHECK(arrow_icon_);
    arrow_icon_->SetImage(ui::ImageModel::FromVectorIcon(
        kQuickSettingsRightArrowIcon, enabled ? cros_tokens::kCrosSysOnSurface
                                              : cros_tokens::kCrosSysDisabled));
  }

  void UpdateSubtitle(bool is_feature_enabled) {
    const auto string_id =
        is_feature_enabled
            ? IDS_ASH_GAME_DASHBOARD_GC_DETAILS_SUB_TITLE_ON_TEMPLATE
            : IDS_ASH_GAME_DASHBOARD_GC_DETAILS_SUB_TITLE_OFF_TEMPLATE;
    header_->UpdateSubtitle(
        l10n_util::GetStringFUTF16(string_id, base::UTF8ToUTF16(app_name_)));

    // In case the sub-title turns to two lines from one line.
    if (GetWidget()) {
      main_menu_->SizeToContents();
    }
  }

  void CacheAppName() {
    if (std::string* app_id = GetGameWindow()->GetProperty(kAppIDKey)) {
      app_name_ = GameDashboardController::Get()->GetArcAppName(*app_id);
    }
  }

  void EnableEditMode() {
    auto* game_window = GetGameWindow();
    const auto flags = game_dashboard_utils::GetGameControlsFlag(game_window);
    CHECK(flags);
    game_window->SetProperty(
        kArcGameControlsFlagsKey,
        game_dashboard_utils::UpdateFlag(*flags, ArcGameControlsFlag::kEdit,
                                         /*enable_flag=*/true));
    const auto& app_id = main_menu_->context_->app_id();
    RecordGameDashboardEditControlsWithEmptyState(
        app_id,
        game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEmpty));
    RecordGameDashboardFunctionTriggered(
        app_id, GameDashboardFunction::kGameControlsSetupOrEdit);

    // Always close the main menu in the end in case of the race condition that
    // this instance is destroyed before the following calls.
    main_menu_->context_->CloseMainMenu(
        GameDashboardMainMenuToggleMethod::kActivateNewFeature);
  }

  aura::Window* GetGameWindow() { return main_menu_->context_->game_window(); }

  // Adds pulse animation and an education nudge for
  // `game_controls_setup_button_` if it exists, is enabled and not optimized
  // for ChromeOS.
  void MaybeDecorateSetupButton() {
    const auto flags =
        game_dashboard_utils::GetGameControlsFlag(GetGameWindow());
    CHECK(flags);

    if (!setup_button_ || !setup_button_->GetEnabled() ||
        game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kO4C)) {
      return;
    }

    ShowNudgeForSetupButton();
    PerformPulseAnimationForSetupButton(/*pulse_count=*/0);
  }

  // Performs pulse animation for `game_controls_setup_button_`.
  void PerformPulseAnimationForSetupButton(int pulse_count) {
    DCHECK(setup_button_);

    // Destroy the pulse layer if it pulses after `kSetupPulseTimes` times.
    if (pulse_count >= kSetupPulseTimes) {
      gc_setup_button_pulse_layer_.reset();
      return;
    }

    auto* widget = GetWidget();
    DCHECK(widget);

    // Initiate pulse layer if it starts to pulse for the first time.
    if (pulse_count == 0) {
      gc_setup_button_pulse_layer_ =
          std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
      widget->GetLayer()->Add(gc_setup_button_pulse_layer_.get());
      gc_setup_button_pulse_layer_->SetColor(
          widget->GetColorProvider()->GetColor(
              cros_tokens::kCrosSysHighlightText));
    }

    DCHECK(gc_setup_button_pulse_layer_);

    // Initial setup button bounds in its widget coordinate.
    const auto setup_bounds =
        setup_button_->ConvertRectToWidget(gfx::Rect(setup_button_->size()));

    // Set initial properties.
    const float initial_corner_radius = setup_bounds.height() / 2.0f;
    gc_setup_button_pulse_layer_->SetBounds(setup_bounds);
    gc_setup_button_pulse_layer_->SetOpacity(1.0f);
    gc_setup_button_pulse_layer_->SetRoundedCornerRadius(
        gfx::RoundedCornersF(initial_corner_radius));

    // Animate to target bounds, opacity and rounded corner radius.
    auto target_bounds = setup_bounds;
    target_bounds.Outset(kSetupPulseExtraHalfSize);
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .OnEnded(base::BindOnce(
            &GameControlsDetailsRow::PerformPulseAnimationForSetupButton,
            base::Unretained(this), pulse_count + 1))
        .Once()
        .SetDuration(kSetupPulseDuration)
        .SetBounds(gc_setup_button_pulse_layer_.get(), target_bounds,
                   gfx::Tween::ACCEL_0_40_DECEL_100)
        .SetOpacity(gc_setup_button_pulse_layer_.get(), 0.0f,
                    gfx::Tween::ACCEL_0_80_DECEL_80)
        .SetRoundedCorners(gc_setup_button_pulse_layer_.get(),
                           gfx::RoundedCornersF(initial_corner_radius +
                                                kSetupPulseExtraHalfSize),
                           gfx::Tween::ACCEL_0_40_DECEL_100);
  }

  // Shows education nudge for `game_controls_setup_button_`.
  void ShowNudgeForSetupButton() {
    DCHECK(setup_button_);

    auto nudge_data = AnchoredNudgeData(
        kSetupNudgeId, NudgeCatalogName::kGameDashboardControlsNudge,
        l10n_util::GetStringUTF16(
            IDS_ASH_GAME_DASHBOARD_GC_KEYBOARD_SETUP_NUDGE_SUB_TITLE),
        this);
    nudge_data.image_model =
        ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
            IDR_GAME_DASHBOARD_CONTROLS_SETUP_NUDGE);
    nudge_data.title_text = l10n_util::GetStringUTF16(
        IDS_ASH_GAME_DASHBOARD_GC_KEYBOARD_SETUP_NUDGE_TITLE);
    nudge_data.arrow = views::BubbleBorder::LEFT_CENTER;
    nudge_data.background_color_id = cros_tokens::kCrosSysBaseHighlight;
    nudge_data.image_background_color_id = cros_tokens::kCrosSysOnBaseHighlight;
    nudge_data.duration = NudgeDuration::kMediumDuration;
    nudge_data.highlight_anchor_button = false;

    Shell::Get()->anchored_nudge_manager()->Show(nudge_data);
  }

  // Removes the setup button pulse animation and nudge if there is any.
  void RemoveSetupButtonDecorationIfAny() {
    gc_setup_button_pulse_layer_.reset();
    Shell::Get()->anchored_nudge_manager()->Cancel(kSetupNudgeId);
  }

  const raw_ptr<GameDashboardMainMenuView> main_menu_;

  raw_ptr<FeatureHeader> header_ = nullptr;
  raw_ptr<PillButton> setup_button_ = nullptr;
  raw_ptr<Switch> feature_switch_ = nullptr;
  raw_ptr<views::ImageView> arrow_icon_ = nullptr;

  // App name from the app where this view is anchored.
  std::string app_name_;

  // Layer for setup button pulse animation.
  std::unique_ptr<ui::Layer> gc_setup_button_pulse_layer_;
};

BEGIN_METADATA(GameDashboardMainMenuView, GameControlsDetailsRow)
END_METADATA

// -----------------------------------------------------------------------------
// GameDashboardMainMenuView:

GameDashboardMainMenuView::GameDashboardMainMenuView(
    GameDashboardContext* context)
    : context_(context) {
  DCHECK(context_);
  DCHECK(context_->game_dashboard_button_widget());

  SetBorder(views::CreateRoundedRectBorder(
      /*thickness=*/1, kBubbleCornerRadius,
      cros_tokens::kCrosSysSystemHighlight1));
  set_corner_radius(kBubbleCornerRadius);
  // Closing on deactivation is manually handled by the `GameDashboardContext`
  // in order to support tabbing between sibling widgets.
  set_close_on_deactivate(false);
  set_internal_name("GameDashboardMainMenuView");
  set_margins(gfx::Insets());
  set_parent_window(
      context_->game_dashboard_button_widget()->GetNativeWindow());
  set_fixed_width(kMainMenuFixedWidth);
  SetAnchorView(context_->game_dashboard_button_widget()->GetContentsView());
  SetArrow(views::BubbleBorder::Arrow::NONE);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kPaddingHeight, kPaddingWidth),
      /*between_child_spacing=*/16));

  // TODO(b/326259321): Move the main menu view and settings view panels into
  // separate class containers and show/hide the view containers
  AddMainMenuViews();

  SizeToPreferredSize();

  // We set the dialog role because views::BubbleDialogDelegate defaults this to
  // an alert dialog. This would make screen readers announce every view in the
  // main menu, which is undesirable.
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);
  SetAccessibleTitle(l10n_util::GetStringUTF16(
      IDS_ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_TITLE));
}

GameDashboardMainMenuView::~GameDashboardMainMenuView() = default;

void GameDashboardMainMenuView::OnRecordingStarted(
    bool is_recording_game_window) {
  UpdateRecordGameTile(is_recording_game_window);
}

void GameDashboardMainMenuView::OnRecordingEnded() {
  UpdateRecordGameTile(/*is_recording_game_window=*/false);
}

void GameDashboardMainMenuView::UpdateRecordingDuration(
    const std::u16string& duration) {
  record_game_tile_->SetSubLabel(duration);
}

void GameDashboardMainMenuView::OnToolbarTilePressed() {
  bool toolbar_visible = context_->ToggleToolbar();
  game_dashboard_utils::SetShowToolbar(toolbar_visible);
  toolbar_tile_->SetSubLabel(
      toolbar_visible
          ? l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_VISIBLE_STATUS)
          : l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_HIDDEN_STATUS));
  toolbar_tile_->SetToggled(toolbar_visible);
  toolbar_tile_->SetTooltipText(l10n_util::GetStringUTF16(
      toolbar_tile_->IsToggled()
          ? IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_TOOLTIPS_HIDE_TOOLBAR
          : IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_TOOLTIPS_SHOW_TOOLBAR));
}

void GameDashboardMainMenuView::OnRecordGameTilePressed() {
  context_->set_recording_from_main_menu(true);

  if (record_game_tile_->IsToggled()) {
    CaptureModeController::Get()->EndVideoRecording(
        EndRecordingReason::kGameDashboardStopRecordingButton);
  } else {
    // Post a task to start a capture session, after the main menu widget
    // closes. When the main menu opens, `GameDashboardContext` registers
    // `GameDashboardMainMenuCursorHandler` as a pretarget handler to always
    // show the mouse cursor. `GameDashboardMainMenuCursorHandler` gets the
    // `wm::CursorManager`, makes the mouse cursor visible, and locks it. This
    // is to prevent other components from changing it.
    // `CaptureModeController::StartForGameDashboard()` also locks the mouse
    // cursor in a similar fashion. The nested locking/unlocking has an
    // undesirable behavior. Starting the capture session in a different task
    // makes the lock/unlock behavior in `wm::CursorManager` occur serially.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<GameDashboardContext> context) {
                         if (context) {
                           GameDashboardController::Get()->StartCaptureSession(
                               context.get());
                         }
                       },
                       context_->GetWeakPtr()));

    // Always close the main menu in the end in case of the race condition that
    // this instance is destroyed before the following calls.
    context_->CloseMainMenu(
        GameDashboardMainMenuToggleMethod::kActivateNewFeature);
  }
}

void GameDashboardMainMenuView::OnScreenshotTilePressed() {
  auto* game_window = context_->game_window();
  CaptureModeController::Get()->CaptureScreenshotOfGivenWindow(game_window);

  RecordGameDashboardScreenshotTakeSource(context_->app_id(),
                                          GameDashboardMenu::kMainMenu);

  // Always close the main menu in the end in case of the race condition that
  // this instance is destroyed before the following calls.
  context_->CloseMainMenu(
      GameDashboardMainMenuToggleMethod::kActivateNewFeature);
}

void GameDashboardMainMenuView::OnSettingsBackButtonPressed() {
  DCHECK(settings_view_container_ && main_menu_container_);
  DCHECK(settings_view_container_->GetVisible() &&
         !main_menu_container_->GetVisible());
  settings_view_container_->SetVisible(false);
  main_menu_container_->SetVisible(true);
  SizeToContents();
  RecordGameDashboardFunctionTriggered(context_->app_id(),
                                       GameDashboardFunction::kSettingBack);
}

void GameDashboardMainMenuView::OnWelcomeDialogSwitchPressed() {
  const bool new_state = welcome_dialog_settings_switch_->GetIsOn();
  game_dashboard_utils::SetShowWelcomeDialog(new_state);
  OnWelcomeDialogSwitchStateChanged(new_state);
  RecordGameDashboardWelcomeDialogNotificationToggleState(context_->app_id(),
                                                          new_state);
}

void GameDashboardMainMenuView::OnGameControlsTilePressed() {
  auto* game_window = context_->game_window();
  const bool was_toggled = game_controls_tile_->IsToggled();
  game_window->SetProperty(
      kArcGameControlsFlagsKey,
      game_dashboard_utils::UpdateFlag(
          game_window->GetProperty(kArcGameControlsFlagsKey),
          ArcGameControlsFlag::kHint,
          /*enable_flag=*/!was_toggled));
  UpdateGameControlsTile();
  RecordGameDashboardControlsHintToggleSource(
      context_->app_id(), GameDashboardMenu::kMainMenu, !was_toggled);
}

void GameDashboardMainMenuView::UpdateGameControlsTile() {
  DCHECK(game_controls_tile_);

  const auto flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  CHECK(flags);

  game_dashboard_utils::UpdateGameControlsHintButton(game_controls_tile_,
                                                     *flags);
}

void GameDashboardMainMenuView::OnScreenSizeSettingsButtonPressed() {
  GameDashboardController::Get()->ShowResizeToggleMenu(context_->game_window());
  RecordGameDashboardFunctionTriggered(context_->app_id(),
                                       GameDashboardFunction::kScreenSize);

  // Always close the main menu in the end in case of the race condition that
  // this instance is destroyed before the following calls.
  context_->CloseMainMenu(
      GameDashboardMainMenuToggleMethod::kActivateNewFeature);
}

void GameDashboardMainMenuView::OnFeedbackButtonPressed() {
  Shell::Get()->shell_delegate()->OpenFeedbackDialog(
      ShellDelegate::FeedbackSource::kGameDashboard,
      /*description_template=*/"#GameDashboard\n\n",
      /*category_tag=*/std::string());
  RecordGameDashboardFunctionTriggered(context_->app_id(),
                                       GameDashboardFunction::kFeedback);
}

void GameDashboardMainMenuView::OnHelpButtonPressed() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kHelpUrl), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  RecordGameDashboardFunctionTriggered(context_->app_id(),
                                       GameDashboardFunction::kHelp);
}

void GameDashboardMainMenuView::OnSettingsButtonPressed() {
  DCHECK(main_menu_container_ && main_menu_container_->GetVisible());
  main_menu_container_->SetVisible(false);
  if (settings_view_container_) {
    settings_view_container_->SetVisible(true);
  } else {
    AddSettingsViews();
  }
  SizeToContents();
  RecordGameDashboardFunctionTriggered(context_->app_id(),
                                       GameDashboardFunction::kSetting);
}

void GameDashboardMainMenuView::AddMainMenuViews() {
  DCHECK(!main_menu_container_);
  main_menu_container_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  main_menu_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  main_menu_container_->SetBetweenChildSpacing(16);

  AddShortcutTilesRow();
  MaybeAddArcFeatureRows();
  AddUtilityClusterRow();
}

void GameDashboardMainMenuView::AddShortcutTilesRow() {
  DCHECK(main_menu_container_);
  views::BoxLayoutView* container = main_menu_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container->SetBetweenChildSpacing(kCenterPadding);

  std::optional<ArcGameControlsFlag> game_controls_flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  const bool record_feature_enabled = base::FeatureList::IsEnabled(
      features::kFeatureManagementGameDashboardRecordGame);

  // Determines the tile type to assign to all Feature Tiles. There will be at
  // least 2 tiles. In cases that there are more, the tile type is set to
  // 'FeatureTile::TileType::kCompact', and if not,
  // 'FeatureTile::TileType::kPrimary'.
  const FeatureTile::TileType tile_type =
      (game_controls_flags || record_feature_enabled)
          ? FeatureTile::TileType::kCompact
          : FeatureTile::TileType::kPrimary;

  const bool toolbar_visible = context_->IsToolbarVisible();
  toolbar_tile_ = container->AddChildView(CreateFeatureTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnToolbarTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, tile_type, VIEW_ID_GD_TOOLBAR_TILE, kGdToolbarIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_BUTTON_TITLE),
      toolbar_visible
          ? l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_VISIBLE_STATUS)
          : l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_HIDDEN_STATUS)));
  toolbar_tile_->SetToggled(toolbar_visible);
  toolbar_tile_->SetTooltipText(l10n_util::GetStringUTF16(
      toolbar_tile_->IsToggled()
          ? IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_TOOLTIPS_HIDE_TOOLBAR
          : IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_TOOLTIPS_SHOW_TOOLBAR));

  if (game_controls_flags) {
    AddGameControlsTile(container, tile_type);
  }

  if (record_feature_enabled) {
    AddRecordGameTile(container, tile_type);
  }

  auto* screenshot_tile = container->AddChildView(CreateFeatureTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnScreenshotTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, tile_type, VIEW_ID_GD_SCREENSHOT_TILE,
      kGdScreenshotIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_SCREENSHOT_TILE_BUTTON_TITLE),
      /*sub_label=*/std::nullopt));
  // `screenshot_tile` is treated as a button instead of toggle button here.
  screenshot_tile->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  // Remove the sub-label view from Screenshot Feature Tile.
  if (tile_type == FeatureTile::TileType::kPrimary) {
    screenshot_tile->sub_label()->SetVisible(false);
  }

  // Shortcut Tiles row holds up to 4 tiles. Set the padding accordingly to
  // the amount of tiles in the Shortcut Tiles row.
  auto tiles = container->children();
  const auto tile_count = tiles.size();
  DCHECK(tile_count >= 2 && tile_count <= 4);
  const auto title_container_margin = tile_count == 4   ? kFourTilePadding
                                      : tile_count == 3 ? kThreeTilePadding
                                                        : kTwoTilePadding;
  for (auto tile : tiles) {
    // Ensure that the Feature Tiles stretch out to equal width and height in
    // the Feature Tile row.
    tile->SetPreferredSize(gfx::Size(1, tile->GetPreferredSize().height()));
    // Adjust padding for depending on tile quantity to prevent clipping.
    views::AsViewClass<FeatureTile>(tile)->SetTitleContainerMargins(
        title_container_margin);
  }

  container->SetDefaultFlex(1);
}

void GameDashboardMainMenuView::MaybeAddArcFeatureRows() {
  const aura::Window* game_window = context_->game_window();
  if (!IsArcWindow(game_window)) {
    return;
  }
  DCHECK(main_menu_container_);
  auto* feature_details_container =
      main_menu_container_->AddChildView(std::make_unique<views::View>());
  feature_details_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          /*inside_border_insets=*/gfx::Insets(),
          /*between_child_spacing=*/2));
  const std::optional<ArcGameControlsFlag> flags =
      game_dashboard_utils::GetGameControlsFlag(game_window);
  const bool has_multi_rows =
      !game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kO4C);
  AddGameControlsDetailsRow(feature_details_container, has_multi_rows
                                                           ? kTopMultiRowCorners
                                                           : kSingleRowCorners);
  if (has_multi_rows) {
    // Only add the Screen Size row if an app is NOT O4C.
    AddScreenSizeSettingsRow(feature_details_container);
  }
}

void GameDashboardMainMenuView::AddGameControlsTile(
    views::View* container,
    FeatureTile::TileType tile_type) {
  DCHECK(game_dashboard_utils::GetGameControlsFlag(context_->game_window()));

  // Add the game controls tile which shows and hides the game controls
  // mapping hint.
  game_controls_tile_ = container->AddChildView(CreateFeatureTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnGameControlsTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, tile_type, VIEW_ID_GD_CONTROLS_TILE,
      kGdGameControlsIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_CONTROLS_TILE_BUTTON_TITLE),
      /*sub_label=*/std::nullopt));
  UpdateGameControlsTile();

  // Call `SetSubLabelVisibility` after the sub-label is set.
  game_controls_tile_->SetSubLabelVisibility(true);
}

void GameDashboardMainMenuView::AddRecordGameTile(
    views::View* container,
    FeatureTile::TileType tile_type) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kFeatureManagementGameDashboardRecordGame));

  record_game_tile_ = container->AddChildView(CreateFeatureTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnRecordGameTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, tile_type, VIEW_ID_GD_RECORD_GAME_TILE,
      kGdRecordGameIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_BUTTON_TITLE),
      /*sub_label=*/std::nullopt));
  // Set toggled background color.
  record_game_tile_->SetBackgroundToggledColorId(
      cros_tokens::kCrosSysSystemNegativeContainer);

  // Set the label's foreground toggled colors.
  record_game_tile_->SetForegroundToggledColorId(
      cros_tokens::kCrosSysSystemOnNegativeContainer);
  // Set the sub-label's foreground toggled colors.
  record_game_tile_->SetForegroundOptionalToggledColorId(
      cros_tokens::kCrosSysSystemOnNegativeContainer);

  // Set toggled ink drop color.
  record_game_tile_->SetInkDropToggledBaseColorId(
      cros_tokens::kCrosSysRippleNeutralOnProminent);
  UpdateRecordGameTile(
      GameDashboardController::Get()->active_recording_context() == context_);
}

void GameDashboardMainMenuView::AddGameControlsDetailsRow(
    views::View* container,
    const gfx::RoundedCornersF& row_corners) {
  DCHECK(IsArcWindow(context_->game_window()));
  game_controls_details_ = container->AddChildView(
      std::make_unique<GameControlsDetailsRow>(this, row_corners));
}

void GameDashboardMainMenuView::AddScreenSizeSettingsRow(
    views::View* container) {
  aura::Window* game_window = context_->game_window();
  DCHECK(IsArcWindow(game_window));
  screen_size_row_ = container->AddChildView(std::make_unique<ScreenSizeRow>(
      this,
      base::BindRepeating(
          &GameDashboardMainMenuView::OnScreenSizeSettingsButtonPressed,
          base::Unretained(this)),
      /*resize_mode=*/compat_mode_util::PredictCurrentMode(game_window),
      /*resize_lock_type=*/game_window->GetProperty(kArcResizeLockTypeKey)));
}

void GameDashboardMainMenuView::AddUtilityFeatureViews(views::View* container) {
  // Add clock view.
  clock_view_ = container->AddChildView(std::make_unique<TimeView>(
      TimeView::ClockLayout::HORIZONTAL_CLOCK,
      Shell::Get()->system_tray_model()->clock(), TimeView::kTime));
  clock_view_->SetAmPmClockType(base::AmPmClockType::kKeepAmPm);
  clock_view_->SetProperty(views::kMarginsKey, kClockViewPadding);

  // Add battery view.
  battery_view_ =
      container->AddChildView(std::make_unique<GameDashboardBatteryView>());
  battery_view_->SetProperty(views::kMarginsKey, gfx::Insets::VH(10, 0));
  battery_view_->SetTooltipText(PowerStatus::Get()->GetInlinedStatusString());
}

void GameDashboardMainMenuView::AddUtilityClusterRow() {
  DCHECK(main_menu_container_);
  auto* container =
      main_menu_container_->AddChildView(std::make_unique<views::View>());
  auto* layout = container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      /*inside_border_insets=*/gfx::Insets(),
      /*between_child_spacing=*/16));

  // The clock and battery icons increase the height of the utility cluster row.
  // Centering the elements inside the row prevents them from stretching or
  // sticking to the boundaries of the row as its size changes.
  layout->set_cross_axis_alignment(views::LayoutAlignment::kCenter);

  auto* feedback_button =
      container->AddChildView(std::make_unique<ash::PillButton>(
          base::BindRepeating(
              &GameDashboardMainMenuView::OnFeedbackButtonPressed,
              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_ASH_GAME_DASHBOARD_SEND_FEEDBACK_TITLE)));
  feedback_button->SetID(VIEW_ID_GD_FEEDBACK_BUTTON);

  // `feedback_button` should be left aligned. Help button and setting button
  // should be right aligned. So add an empty view to fill the empty space.
  auto* empty_view = container->AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(empty_view, /*flex=*/1);

  if (features::AreGameDashboardUtilitiesEnabled()) {
    AddUtilityFeatureViews(
        container->AddChildView(std::make_unique<views::BoxLayoutView>()));
  }

  auto* help_button = container->AddChildView(CreateIconButton(
      base::BindRepeating(&GameDashboardMainMenuView::OnHelpButtonPressed,
                          base::Unretained(this)),
      VIEW_ID_GD_HELP_BUTTON, kGdHelpIcon,
      l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_HELP_TOOLTIP)));
  help_button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_HELP_BUTTON_A11Y_LABEL));
  container->AddChildView(CreateIconButton(
      base::BindRepeating(&GameDashboardMainMenuView::OnSettingsButtonPressed,
                          base::Unretained(this)),
      VIEW_ID_GD_GENERAL_SETTINGS_BUTTON, kGdSettingsIcon,
      l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_SETTINGS_TOOLTIP)));
}

void GameDashboardMainMenuView::VisibilityChanged(views::View* starting_from,
                                                  bool is_visible) {
  // When the menu shows up, Game Controls shouldn't rewrite events. So Game
  // Controls needs to know when the menu is open or closed.
  auto flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  if (!flags || !game_dashboard_utils::IsFlagSet(
                    *flags, ArcGameControlsFlag::kAvailable)) {
    return;
  }

  context_->game_window()->SetProperty(
      kArcGameControlsFlagsKey,
      game_dashboard_utils::UpdateFlag(*flags, ArcGameControlsFlag::kMenu,
                                       /*enable_flag=*/is_visible));
}

void GameDashboardMainMenuView::UpdateRecordGameTile(
    bool is_recording_game_window) {
  if (!record_game_tile_) {
    return;
  }

  record_game_tile_->SetEnabled(
      is_recording_game_window ||
      CaptureModeController::Get()->can_start_new_recording());

  record_game_tile_->SetVectorIcon(is_recording_game_window
                                       ? kCaptureModeCircleStopIcon
                                       : kGdRecordGameIcon);
  record_game_tile_->SetLabel(l10n_util::GetStringUTF16(
      is_recording_game_window
          ? IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_BUTTON_RECORDING_TITLE
          : IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_BUTTON_TITLE));
  if (is_recording_game_window) {
    record_game_tile_->SetSubLabel(context_->GetRecordingDuration());
  }
  record_game_tile_->SetSubLabelVisibility(is_recording_game_window);
  record_game_tile_->SetToggled(is_recording_game_window);
  record_game_tile_->SetTooltipText(l10n_util::GetStringUTF16(
      record_game_tile_->IsToggled()
          ? IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_TOOLTIPS_RECORD_STOP
          : IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_TOOLTIPS_RECORD_START));
}

void GameDashboardMainMenuView::AddSettingsViews() {
  DCHECK(!settings_view_container_);
  settings_view_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  settings_view_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  settings_view_container_->SetBetweenChildSpacing(16);

  AddSettingsTitleRow();
  AddWelcomeDialogSettingsRow();
}

void GameDashboardMainMenuView::AddSettingsTitleRow() {
  auto* title_container = settings_view_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  title_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  title_container->SetInsideBorderInsets(
      gfx::Insets::TLBR(0, 0, 0, /*padding to offset back button size=*/32));

  // Add back button to the title container.
  settings_view_back_button_ =
      title_container->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(
              &GameDashboardMainMenuView::OnSettingsBackButtonPressed,
              base::Unretained(this)),
          IconButton::Type::kMedium, &kQuickSettingsLeftArrowIcon,
          IDS_ASH_GAME_DASHBOARD_BACK_TOOLTIP));

  // Add title label to the title container.
  auto* title = title_container->AddChildView(bubble_utils::CreateLabel(
      TypographyToken::kCrosTitle1,
      l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_SETTINGS_TITLE),
      cros_tokens::kCrosSysOnSurface));
  title->SetMultiLine(true);
  title->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  // Flex `title` to fill empty space in row.
  title_container->SetFlexForView(title, /*flex=*/1);
}

void GameDashboardMainMenuView::AddWelcomeDialogSettingsRow() {
  auto* welcome_settings_container = settings_view_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  welcome_settings_container->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  welcome_settings_container->SetInsideBorderInsets(gfx::Insets::VH(16, 16));
  welcome_settings_container->SetBackground(
      views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysSystemOnBase, kBubbleCornerRadius));

  // Add icon.
  auto* icon_container = welcome_settings_container->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  icon_container->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase,
      /*radius=*/12.0f));
  icon_container->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(6, 6)));
  icon_container->SetProperty(views::kMarginsKey,
                              gfx::Insets::TLBR(0, 0, 0, 16));
  icon_container->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kGdNotificationIcon, cros_tokens::kCrosSysOnSurface,
          /*icon_size=*/20)));

  // Add title.
  auto* feature_title = welcome_settings_container->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_SETTINGS_WELCOME_DIALOG_TITLE)));
  feature_title->SetAutoColorReadabilityEnabled(false);
  feature_title->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  feature_title->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosTitle2));
  feature_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  feature_title->SetMultiLine(true);
  feature_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Flex `feature_title` to fill empty space in row.
  welcome_settings_container->SetFlexForView(feature_title, /*flex=*/1);

  // Add welcome dialog switch.
  welcome_dialog_settings_switch_ = welcome_settings_container->AddChildView(
      std::make_unique<Switch>(base::BindRepeating(
          &GameDashboardMainMenuView::OnWelcomeDialogSwitchPressed,
          base::Unretained(this))));
  const bool is_enabled = game_dashboard_utils::ShouldShowWelcomeDialog();
  OnWelcomeDialogSwitchStateChanged(is_enabled);
  welcome_dialog_settings_switch_->SetProperty(views::kMarginsKey,
                                               gfx::Insets::TLBR(0, 8, 0, 0));
  welcome_dialog_settings_switch_->SetIsOn(is_enabled);
}

void GameDashboardMainMenuView::OnWelcomeDialogSwitchStateChanged(
    bool is_enabled) {
  welcome_dialog_settings_switch_->GetViewAccessibility().SetName(
      l10n_util::GetStringFUTF16(
          IDS_ASH_GAME_DASHBOARD_SETTINGS_WELCOME_DIALOG_A11Y_LABEL,
          l10n_util::GetStringUTF16(is_enabled
                                        ? IDS_ASH_GAME_DASHBOARD_TILE_ON
                                        : IDS_ASH_GAME_DASHBOARD_GC_TILE_OFF)));
}

PillButton* GameDashboardMainMenuView::GetGameControlsSetupButton() {
  return game_controls_details_ ? game_controls_details_->setup_button()
                                : nullptr;
}

Switch* GameDashboardMainMenuView::GetGameControlsFeatureSwitch() {
  return game_controls_details_ ? game_controls_details_->feature_switch()
                                : nullptr;
}

AnchoredNudge*
GameDashboardMainMenuView::GetGameControlsSetupNudgeForTesting() {
  if (Shell::Get()->anchored_nudge_manager()->IsNudgeShown(kSetupNudgeId)) {
    return Shell::Get()
        ->anchored_nudge_manager()
        ->GetShownNudgeForTest(  // IN-TEST
            kSetupNudgeId);
  }
  return nullptr;
}

const views::Label* GameDashboardMainMenuView::GetScreenSizeRowSubtitle() {
  return screen_size_row_ ? screen_size_row_->feature_header()->GetSubtitle()
                          : nullptr;
}

void GameDashboardMainMenuView::OnThemeChanged() {
  views::View::OnThemeChanged();
  set_color(GetColorProvider()->GetColor(
      cros_tokens::kCrosSysSystemBaseElevatedOpaque));
}

BEGIN_METADATA(GameDashboardMainMenuView)
END_METADATA

}  // namespace ash
