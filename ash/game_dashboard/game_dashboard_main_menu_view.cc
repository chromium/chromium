// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_main_menu_view.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/style_util.h"
#include "ash/style/switch.h"
#include "ash/style/typography.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kBubbleCornerRadius = 8;
// Horizontal padding for the border around the main menu.
constexpr int kPaddingWidth = 12;
// Vertical padding for the border around the main menu.
constexpr int kPaddingHeight = 15;
// Padding between children in a row or column.
constexpr int kCenterPadding = 8;

// Creates an individual Game Dashboard Tile.
std::unique_ptr<FeatureTile> CreateTile(base::RepeatingClosure callback,
                                        bool is_togglable,
                                        FeatureTile::TileType type,
                                        int id,
                                        const gfx::VectorIcon& icon,
                                        const std::u16string& text) {
  auto tile =
      std::make_unique<FeatureTile>(std::move(callback), is_togglable, type);
  tile->SetID(id);
  tile->SetVectorIcon(icon);
  tile->SetLabel(text);
  tile->SetTooltipText(text);
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

}  // namespace

// -----------------------------------------------------------------------------
// GameDashboardMainMenuView::FeatureDetailsRow:

// Feature details row includes feature icon, title and sub-title, drill in
// arrow icon as the tail view or customized tail view. The row looks like:
// +----------------------------------+
// | |icon|  |title|       |tail_view||
// |         |sub-title|              |
// +----------------------------------+
class GameDashboardMainMenuView::FeatureDetailsRow : public views::Button {
 public:
  FeatureDetailsRow(base::RepeatingCallback<void()> callback,
                    RoundedContainer::Behavior corner_behavior,
                    bool default_drill_in_arrow,
                    const gfx::VectorIcon& icon,
                    const std::u16string& title)
      : Button(callback) {
    SetAccessibleName(title);
    SetTooltipText(title);
    SetUseDefaultFillLayout(true);

    // Create 1x3 table. TableLayout is used because the first two columns are
    // aligned to left and the last column is aligned to right.
    auto* container = AddChildView(std::make_unique<RoundedContainer>(
        corner_behavior, /*non_rounded_radius=*/0));
    container->SetLayoutManager(std::make_unique<views::TableLayout>())
        ->AddColumn(/*h_align=*/views::LayoutAlignment::kStart,
                    /*v_align=*/views::LayoutAlignment::kCenter,
                    /*horizontal_resize=*/views::TableLayout::kFixedSize,
                    /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                    /*fixed_width=*/0, /*min_width=*/0)
        .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                          /*width=*/16)
        .AddColumn(/*h_align=*/views::LayoutAlignment::kStretch,
                   /*v_align=*/views::LayoutAlignment::kCenter,
                   /*horizontal_resize=*/1.0f,
                   /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                   /*fixed_width=*/0, /*min_width=*/0)
        .AddColumn(/*h_align=*/views::LayoutAlignment::kEnd,
                   /*v_align=*/views::LayoutAlignment::kCenter,
                   /*horizontal_resize=*/views::TableLayout::kFixedSize,
                   /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                   /*fixed_width=*/0, /*min_width=*/0)
        .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize);
    container->SetBorderInsets(gfx::Insets::VH(16, 14));

    views::HighlightPathGenerator::Install(
        this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                  gfx::Insets(), container->layer()->rounded_corner_radii()));

    // Add icon.
    auto* icon_container =
        container->AddChildView(std::make_unique<views::View>());
    icon_container->SetLayoutManager(std::make_unique<views::FillLayout>());
    icon_container->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase, /*radius=*/12));
    icon_container->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(6, 6)));
    icon_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            icon, cros_tokens::kCrosSysOnSurface, 20)));

    // Add title and sub-title.
    auto* tag_container =
        container->AddChildView(std::make_unique<views::BoxLayoutView>());
    tag_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
    tag_container->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    // Add title.
    auto* feature_title =
        tag_container->AddChildView(std::make_unique<views::Label>(title));
    feature_title->SetAutoColorReadabilityEnabled(false);
    feature_title->SetEnabledColorId(cros_tokens::kCrosRefNeutral100);
    feature_title->SetFontList(
        TypographyProvider::Get()->ResolveTypographyToken(
            TypographyToken::kCrosTitle2));
    // Add sub-title.
    sub_title_ = tag_container->AddChildView(
        bubble_utils::CreateLabel(TypographyToken::kCrosAnnotation2, u"",
                                  cros_tokens::kCrosSysOnSurfaceVariant));

    // Add tail view.
    tail_container_ = container->AddChildView(std::make_unique<views::View>());
    tail_container_->SetUseDefaultFillLayout(true);
    if (default_drill_in_arrow) {
      tail_container_->AddChildView(
          std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
              kQuickSettingsRightArrowIcon, cros_tokens::kCrosSysOnSurface)));
    }
  }

  FeatureDetailsRow(const FeatureDetailsRow&) = delete;
  FeatureDetailsRow& operator=(const FeatureDetailsRow) = delete;
  ~FeatureDetailsRow() override = default;

  void SetSubtitle(const std::u16string& sub_title) {
    sub_title_->SetText(sub_title);
  }

  template <typename T>
  T* AddCustomizedTailView(std::unique_ptr<T> view) {
    tail_container_->RemoveAllChildViews();
    return tail_container_->AddChildView(std::move(view));
  }

 private:
  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    // Set up highlight and focus ring for the whole row.
    StyleUtil::SetUpInkDropForButton(
        /*button=*/this, gfx::Insets(), /*highlight_on_hover=*/true,
        /*highlight_on_focus=*/true, /*background_color=*/
        GetColorProvider()->GetColor(cros_tokens::kCrosSysHoverOnSubtle));

    // `StyleUtil::SetUpInkDropForButton()` reinstalls the focus ring, so it
    // needs to set the focus ring size after calling
    // `StyleUtil::SetUpInkDropForButton()`.
    auto* focus_ring = views::FocusRing::Get(this);
    focus_ring->SetHaloInset(-4);
    focus_ring->SetHaloThickness(2);
  }

  raw_ptr<views::Label> sub_title_ = nullptr;
  raw_ptr<views::View> tail_container_ = nullptr;
};

// -----------------------------------------------------------------------------
// GameDashboardMainMenuView:

GameDashboardMainMenuView::GameDashboardMainMenuView(
    GameDashboardContext* context)
    : context_(context) {
  DCHECK(context_);
  DCHECK(context_->main_menu_button_widget());

  set_corner_radius(kBubbleCornerRadius);
  set_close_on_deactivate(false);
  set_internal_name("GameDashboardMainMenuView");
  set_margins(gfx::Insets());
  set_parent_window(context_->main_menu_button_widget()->GetNativeWindow());
  SetAnchorView(context_->main_menu_button_widget()->GetContentsView());
  SetArrow(views::BubbleBorder::Arrow::NONE);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kPaddingHeight, kPaddingWidth), kCenterPadding));

  AddShortcutTilesRow();
  AddFeatureDetailsRows();
  AddUtilityClusterRow();

  SizeToPreferredSize();
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
  // TODO(b/295070122): Update `record_game_tile_`'s sub-label text to
  // `duration`.
}

void GameDashboardMainMenuView::OnToolbarTilePressed() {
  toolbar_tile_->SetToggled(context_->ToggleToolbar());
}

void GameDashboardMainMenuView::OnRecordGameTilePressed() {
  if (record_game_tile_->IsToggled()) {
    CaptureModeController::Get()->EndVideoRecording(
        EndRecordingReason::kGameDashboardStopRecordingButton);
  } else {
    context_->CloseMainMenu();
    GameDashboardController::Get()->StartCaptureSession(context_);
  }
}

void GameDashboardMainMenuView::OnScreenshotTilePressed() {
  context_->CloseMainMenu();
  CaptureModeController::Get()->CaptureScreenshotOfGivenWindow(
      context_->game_window());
}

void GameDashboardMainMenuView::OnGameControlsTilePressed() {
  game_controls_tile_->SetToggled(!game_controls_tile_->IsToggled());

  bool is_toggled = game_controls_tile_->IsToggled();
  // TODO(b/274690042): Replace the strings with localized strings.
  game_controls_details_->SetSubtitle(is_toggled ? u"On" : u"Off");
  game_controls_details_->SetEnabled(is_toggled);

  if (game_controls_setup_button_) {
    game_controls_setup_button_->SetEnabled(is_toggled);
  } else {
    DCHECK(game_controls_hint_switch_);
    game_controls_hint_switch_->SetEnabled(is_toggled);
    game_controls_hint_switch_->SetIsOn(is_toggled);
  }

  auto* game_window = context_->game_window();
  game_window->SetProperty(
      kArcGameControlsFlagsKey,
      game_dashboard_utils::UpdateFlag(
          game_window->GetProperty(kArcGameControlsFlagsKey),
          static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kEnabled |
                                           ArcGameControlsFlag::kHint),
          /*enable_flag=*/is_toggled));
}

void GameDashboardMainMenuView::OnGameControlsDetailsPressed() {
  EnableGameControlsEditMode();
  context_->CloseMainMenu();
}

void GameDashboardMainMenuView::OnGameControlsSetUpButtonPressed() {
  EnableGameControlsEditMode();
  context_->CloseMainMenu();
}

void GameDashboardMainMenuView::OnGameControlsHintSwitchButtonPressed() {
  auto* game_window = context_->game_window();
  game_window->SetProperty(
      kArcGameControlsFlagsKey,
      game_dashboard_utils::UpdateFlag(
          game_window->GetProperty(kArcGameControlsFlagsKey),
          ArcGameControlsFlag::kHint,
          /*enable_flag=*/game_controls_hint_switch_->GetIsOn()));
}

void GameDashboardMainMenuView::OnScreenSizeSettingsButtonPressed() {
  // TODO(b/283988495): Add support when screen size setting is pressed.
}

void GameDashboardMainMenuView::OnFeedbackButtonPressed() {
  // TODO(b/273641035): Add support when feedback button is pressed.
}

void GameDashboardMainMenuView::OnHelpButtonPressed() {
  // TODO(b/273640773): Add support when help button is pressed.
}

void GameDashboardMainMenuView::OnSettingsButtonPressed() {
  // TODO(b/281773221): Add support when settings button is pressed.
}

void GameDashboardMainMenuView::AddShortcutTilesRow() {
  views::BoxLayoutView* container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container->SetBetweenChildSpacing(kCenterPadding);

  toolbar_tile_ = container->AddChildView(CreateTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnToolbarTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, FeatureTile::TileType::kCompact,
      VIEW_ID_GD_TOOLBAR_TILE, kGdToolbarIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_BUTTON_TITLE)));
  toolbar_tile_->SetToggled(context_->IsToolbarVisible());

  MaybeAddGameControlsTile(container);

  if (base::FeatureList::IsEnabled(
          features::kFeatureManagementGameDashboardRecordGame)) {
    record_game_tile_ = container->AddChildView(CreateTile(
        base::BindRepeating(&GameDashboardMainMenuView::OnRecordGameTilePressed,
                            base::Unretained(this)),
        /*is_togglable=*/true, FeatureTile::TileType::kCompact,
        VIEW_ID_GD_RECORD_GAME_TILE, kGdRecordGameIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_BUTTON_TITLE)));
    UpdateRecordGameTile(
        GameDashboardController::Get()->active_recording_context() == context_);
  }

  container->AddChildView(CreateTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnScreenshotTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, FeatureTile::TileType::kCompact,
      VIEW_ID_GD_SCREENSHOT_TILE, kGdScreenshotIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_SCREENSHOT_TILE_BUTTON_TITLE)));
}

void GameDashboardMainMenuView::AddFeatureDetailsRows() {
  auto* feature_details_container =
      AddChildView(std::make_unique<views::View>());
  feature_details_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          /*inside_border_insets=*/gfx::Insets(),
          /*between_child_spacing=*/2));
  MaybeAddGameControlsDetailsRow(feature_details_container);
  MaybeAddScreenSizeSettingsRow(feature_details_container);
}

void GameDashboardMainMenuView::MaybeAddGameControlsTile(
    views::View* container) {
  auto flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  if (!flags) {
    return;
  }

  game_controls_tile_ = container->AddChildView(CreateTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnGameControlsTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, FeatureTile::TileType::kCompact,
      VIEW_ID_GD_CONTROLS_TILE, kGdGameControlsIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_CONTROLS_TILE_BUTTON_TITLE)));

  game_controls_tile_->SetEnabled(
      !game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEmpty));
  if (game_controls_tile_->GetEnabled()) {
    game_controls_tile_->SetToggled(
        game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEnabled));
  }
}

void GameDashboardMainMenuView::MaybeAddGameControlsDetailsRow(
    views::View* container) {
  auto flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  if (!flags) {
    return;
  }

  DCHECK(game_controls_tile_);
  game_controls_details_ =
      container->AddChildView(std::make_unique<FeatureDetailsRow>(
          base::BindRepeating(
              &GameDashboardMainMenuView::OnGameControlsDetailsPressed,
              base::Unretained(this)),
          RoundedContainer::Behavior::kTopRounded,
          /*default_drill_in_arrow=*/false,
          /*icon=*/kGdGameControlsIcon, /*title=*/
          l10n_util::GetStringUTF16(
              IDS_ASH_GAME_DASHBOARD_CONTROLS_TILE_BUTTON_TITLE)));
  game_controls_details_->SetID(VIEW_ID_GD_CONTROLS_DETAILS_ROW);

  const bool is_enabled =
      game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEnabled);
  // TODO(b/279117180): Include application name in the subtitle.
  // TODO(b/274690042): Replace the strings with localized strings.
  game_controls_details_->SetSubtitle(is_enabled ? u"On" : u"Off");
  game_controls_details_->SetEnabled(is_enabled);

  if (game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEmpty)) {
    // Add "Set up" button for empty state.
    // TODO(b/274690042): Replace the strings with localized strings.
    game_controls_setup_button_ = game_controls_details_->AddCustomizedTailView(
        std::make_unique<PillButton>(
            base::BindRepeating(
                &GameDashboardMainMenuView::OnGameControlsSetUpButtonPressed,
                base::Unretained(this)),
            u"Set up", PillButton::Type::kPrimaryWithoutIcon,
            /*icon=*/nullptr));
    game_controls_setup_button_->SetID(VIEW_ID_GD_CONTROLS_SETUP_BUTTON);
    game_controls_setup_button_->SetProperty(views::kMarginsKey,
                                             gfx::Insets::TLBR(0, 20, 0, 0));
    game_controls_setup_button_->SetEnabled(is_enabled);
  } else {
    // Add toggle button and arrow icon for non-empty state.
    auto* edit_container = game_controls_details_->AddCustomizedTailView(
        std::make_unique<views::View>());
    edit_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        /*inside_border_insets=*/gfx::Insets(),
        /*between_child_spacing=*/18));
    edit_container->SetProperty(views::kMarginsKey,
                                gfx::Insets::TLBR(0, 8, 0, 0));

    // Add switch_button to show or hide input mapping hints.
    game_controls_hint_switch_ = edit_container->AddChildView(
        std::make_unique<Switch>(base::BindRepeating(
            &GameDashboardMainMenuView::OnGameControlsHintSwitchButtonPressed,
            base::Unretained(this))));
    game_controls_hint_switch_->SetID(VIEW_ID_GD_CONTROLS_HINT_SWITCH);
    // TODO(b/279117180): Update the accessibility name.
    game_controls_hint_switch_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
    game_controls_hint_switch_->SetProperty(views::kMarginsKey,
                                            gfx::Insets::TLBR(0, 0, 0, 18));
    game_controls_hint_switch_->SetEnabled(is_enabled);
    game_controls_hint_switch_->SetIsOn(
        is_enabled ? game_dashboard_utils::IsFlagSet(*flags,
                                                     ArcGameControlsFlag::kHint)
                   : false);
    // Add arrow icon.
    edit_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            kQuickSettingsRightArrowIcon, cros_tokens::kCrosSysOnSurface)));
  }
}

void GameDashboardMainMenuView::MaybeAddScreenSizeSettingsRow(
    views::View* container) {
  if (IsArcWindow(context_->game_window())) {
    auto* details = container->AddChildView(std::make_unique<FeatureDetailsRow>(
        base::BindRepeating(
            &GameDashboardMainMenuView::OnScreenSizeSettingsButtonPressed,
            base::Unretained(this)),
        RoundedContainer::Behavior::kBottomRounded,
        /*default_drill_in_arrow=*/true,
        /*icon=*/kGdScreenSizeSettingsIcon, /*title=*/
        l10n_util::GetStringUTF16(
            IDS_ASH_GAME_DASHBOARD_SCREEN_SIZE_SETTINGS_TITLE)));

    details->SetID(VIEW_ID_GD_SCREEN_SIZE_TILE);
    // TODO(b/286455407): Update with final localized string.
    // TODO(b/286917169): Dynamically updating the sub-title.
    details->SetSubtitle(u"Landscape");
  }
}

void GameDashboardMainMenuView::AddUtilityClusterRow() {
  views::BoxLayoutView* container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container->SetBetweenChildSpacing(kCenterPadding);

  auto* feedback_button =
      container->AddChildView(std::make_unique<views::LabelButton>(
          base::BindRepeating(
              &GameDashboardMainMenuView::OnFeedbackButtonPressed,
              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_ASH_GAME_DASHBOARD_SEND_FEEDBACK_TITLE)));
  feedback_button->SetID(VIEW_ID_GD_FEEDBACK_BUTTON);
  feedback_button->SetImageLabelSpacing(kCenterPadding);
  feedback_button->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  container->AddChildView(CreateIconButton(
      base::BindRepeating(&GameDashboardMainMenuView::OnHelpButtonPressed,
                          base::Unretained(this)),
      VIEW_ID_GD_HELP_BUTTON, kGdHelpIcon,
      l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_HELP_TOOLTIP)));
  container->AddChildView(CreateIconButton(
      base::BindRepeating(&GameDashboardMainMenuView::OnSettingsButtonPressed,
                          base::Unretained(this)),
      VIEW_ID_GD_GENERAL_SETTINGS_BUTTON, kGdSettingsIcon,
      l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_SETTINGS_TOOLTIP)));
}

void GameDashboardMainMenuView::EnableGameControlsEditMode() {
  auto* game_window = context_->game_window();
  game_window->SetProperty(
      kArcGameControlsFlagsKey,
      game_dashboard_utils::UpdateFlag(
          game_window->GetProperty(kArcGameControlsFlagsKey),
          ArcGameControlsFlag::kEdit, /*enable_flag=*/true));
}

void GameDashboardMainMenuView::VisibilityChanged(views::View* starting_from,
                                                  bool is_visible) {
  // When the menu shows up, Game Controls shouldn't rewrite events. So Game
  // Controls needs to know when the menu is open or closed.
  auto flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  if (!flags) {
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
      !CaptureModeController::Get()->is_recording_in_progress());
  record_game_tile_->SetToggled(is_recording_game_window);
  // TODO(b/273641154): Update record_game_tile_'s UI to reflect the updated
  // state.
}

BEGIN_METADATA(GameDashboardMainMenuView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash
