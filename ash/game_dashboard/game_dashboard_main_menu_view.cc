// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_main_menu_view.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/game_dashboard/game_dashboard_widget.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/arc_compat_mode_util.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/style_util.h"
#include "ash/style/switch.h"
#include "ash/style/typography.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
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
constexpr int kPaddingWidth = 20;
// Vertical padding for the border around the main menu.
constexpr int kPaddingHeight = 20;
// Padding between children in a row or column.
constexpr int kCenterPadding = 8;
// Main Menu fixed width.
constexpr int kMainMenuFixedWidth = 416;
// Background radius.
constexpr float kBackgroundRadius = 12;
// Corner radius for the detail row container.
constexpr int kDetailRowCornerRadius = 16;

// For setup button pulse animation.
constexpr int kSetupPulseExtraHalfSize = 32;
constexpr int kSetupPulseTimes = 3;
constexpr base::TimeDelta kSetupPulseDuration = base::Seconds(2);

constexpr char kSetupNudgeId[] = "SetupNudgeId";

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
  tile->SetID(id);
  tile->SetVectorIcon(icon);
  tile->SetLabel(text);
  tile->SetTooltipText(text);
  if (sub_label.has_value()) {
    tile->SetSubLabel(sub_label.value());
    tile->SetSubLabelVisibility(true);
  }
  if (type == FeatureTile::TileType::kPrimary) {
    // Remove any corner radius because it's set on the container for any
    // primary `FeatureTile` objects.
    tile->SetButtonCornerRadius(0);
  }
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
// TODO(b/308762948): Update name and params now that only Game Controls uses
// this logic.
class GameDashboardMainMenuView::FeatureDetailsRow : public views::Button {
  METADATA_HEADER(FeatureDetailsRow, views::Button)

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
        cros_tokens::kCrosSysSystemOnBase, kBackgroundRadius));
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

BEGIN_METADATA(GameDashboardMainMenuView, FeatureDetailsRow, views::Button)
END_METADATA

// -----------------------------------------------------------------------------
// GameDashboardMainMenuView:

GameDashboardMainMenuView::GameDashboardMainMenuView(
    GameDashboardContext* context)
    : context_(context) {
  DCHECK(context_);
  DCHECK(context_->game_dashboard_button_widget());

  set_corner_radius(kBubbleCornerRadius);
  set_close_on_deactivate(true);
  set_internal_name("GameDashboardMainMenuView");
  set_margins(gfx::Insets());
  set_parent_window(
      context_->game_dashboard_button_widget()->GetNativeWindow());
  set_fixed_width(kMainMenuFixedWidth);
  SetAnchorView(context_->game_dashboard_button_widget()->GetContentsView());
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
  record_game_tile_->SetSubLabel(duration);
}

void GameDashboardMainMenuView::OnToolbarTilePressed() {
  bool toolbar_visible = context_->ToggleToolbar();
  toolbar_tile_->SetSubLabel(
      toolbar_visible
          ? l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_VISIBLE_STATUS)
          : l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_HIDDEN_STATUS));
  toolbar_tile_->SetToggled(toolbar_visible);
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
  auto* game_window = context_->game_window();
  game_window->SetProperty(
      kArcGameControlsFlagsKey,
      game_dashboard_utils::UpdateFlag(
          game_window->GetProperty(kArcGameControlsFlagsKey),
          ArcGameControlsFlag::kHint,
          /*enable_flag=*/!game_controls_tile_->IsToggled()));

  UpdateGameControlsTile();
}

void GameDashboardMainMenuView::OnGameControlsDetailsPressed() {
  const auto flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  DCHECK(flags);

  // Do nothing if Game Controls is disabled.
  if (!game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEnabled)) {
    return;
  }

  EnableGameControlsEditMode();
  context_->CloseMainMenu();
}

void GameDashboardMainMenuView::OnGameControlsSetUpButtonPressed() {
  EnableGameControlsEditMode();
  context_->CloseMainMenu();
}

void GameDashboardMainMenuView::OnGameControlsFeatureSwitchButtonPressed() {
  const bool is_toggled = game_controls_feature_switch_->GetIsOn();
  UpdateGameControlsDetailsSubtitle(/*is_game_controls_enabled=*/is_toggled);

  auto* game_window = context_->game_window();
  game_window->SetProperty(
      kArcGameControlsFlagsKey,
      game_dashboard_utils::UpdateFlag(
          game_window->GetProperty(kArcGameControlsFlagsKey),
          static_cast<ArcGameControlsFlag>(
              /*enable_flag=*/ArcGameControlsFlag::kEnabled |
              ArcGameControlsFlag::kHint),
          is_toggled));

  UpdateGameControlsTile();
}

void GameDashboardMainMenuView::UpdateGameControlsTile() {
  DCHECK(game_controls_tile_);

  const auto flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  DCHECK(flags);

  bool is_enabled =
      game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEnabled);
  bool is_empty =
      game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEmpty);
  bool is_hint_on =
      game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kHint);

  game_controls_tile_->SetEnabled(is_enabled && !is_empty);
  if (game_controls_tile_->GetEnabled()) {
    game_controls_tile_->SetToggled(is_hint_on);
  }

  game_dashboard_utils::UpdateGameControlsHintButtonToolTipText(
      game_controls_tile_, *flags);

  game_controls_tile_->SetSubLabel(l10n_util::GetStringUTF16(
      !is_enabled || is_empty
          ? IDS_ASH_GAME_DASHBOARD_GC_TILE_OFF
          : (is_hint_on ? IDS_ASH_GAME_DASHBOARD_GC_TILE_VISIBLE
                        : IDS_ASH_GAME_DASHBOARD_GC_TILE_HIDDEN)));
  game_controls_tile_->SetSubLabelVisibility(true);
}

void GameDashboardMainMenuView::UpdateGameControlsDetailsSubtitle(
    bool is_game_controls_enabled) {
  // TODO(b/274690042): Replace the strings with localized strings.
  game_controls_details_->SetSubtitle(
      (is_game_controls_enabled ? u"On for " : u"Off for ") +
      base::UTF8ToUTF16(app_name_));
}

void GameDashboardMainMenuView::CacheAppName() {
  auto* window = context_->game_window();
  DCHECK(IsArcWindow(window));
  std::string* app_id = window->GetProperty(kAppIDKey);
  if (app_id) {
    app_name_ = GameDashboardController::Get()->GetArcAppName(*app_id);
  }
}

void GameDashboardMainMenuView::OnScreenSizeSettingsButtonPressed() {
  // TODO(b/283988495): Add support when screen size setting is pressed.
}

void GameDashboardMainMenuView::OnFeedbackButtonPressed() {
  Shell::Get()->shell_delegate()->OpenFeedbackDialog(
      ShellDelegate::FeedbackSource::kGameDashboard,
      /*description_template=*/"#GameDashboard\n\n");
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

  const bool toolbar_visible = context_->IsToolbarVisible();
  toolbar_tile_ = container->AddChildView(CreateFeatureTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnToolbarTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, FeatureTile::TileType::kCompact,
      VIEW_ID_GD_TOOLBAR_TILE, kGdToolbarIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_BUTTON_TITLE),
      toolbar_visible
          ? l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_VISIBLE_STATUS)
          : l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_HIDDEN_STATUS)));
  toolbar_tile_->SetToggled(toolbar_visible);

  MaybeAddGameControlsTile(container);

  if (base::FeatureList::IsEnabled(
          features::kFeatureManagementGameDashboardRecordGame)) {
    record_game_tile_ = container->AddChildView(CreateFeatureTile(
        base::BindRepeating(&GameDashboardMainMenuView::OnRecordGameTilePressed,
                            base::Unretained(this)),
        /*is_togglable=*/true, FeatureTile::TileType::kCompact,
        VIEW_ID_GD_RECORD_GAME_TILE, kGdRecordGameIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_BUTTON_TITLE),
        /*sub_label=*/std::nullopt));
    record_game_tile_->SetBackgroundColorId(
        cros_tokens::kCrosSysSystemOnBaseOpaque);
    record_game_tile_->SetForegroundColorId(cros_tokens::kCrosSysOnSurface);
    record_game_tile_->SetBackgroundToggledColorId(
        cros_tokens::kCrosSysSystemNegativeContainer);
    record_game_tile_->SetForegroundToggledColorId(
        cros_tokens::kCrosSysSystemOnNegativeContainer);
    UpdateRecordGameTile(
        GameDashboardController::Get()->active_recording_context() == context_);
  }

  container->AddChildView(CreateFeatureTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnScreenshotTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, FeatureTile::TileType::kCompact,
      VIEW_ID_GD_SCREENSHOT_TILE, kGdScreenshotIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_SCREENSHOT_TILE_BUTTON_TITLE),
      /*sub_label=*/std::nullopt));
}

void GameDashboardMainMenuView::AddFeatureDetailsRows() {
  auto* feature_details_container =
      AddChildView(std::make_unique<views::View>());
  feature_details_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          /*inside_border_insets=*/gfx::Insets(),
          /*between_child_spacing=*/2));

  // Set the container's corner radius.
  feature_details_container->SetPaintToLayer();
  auto* container_layer = feature_details_container->layer();
  container_layer->SetFillsBoundsOpaquely(false);
  container_layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kDetailRowCornerRadius));

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

  // Add the game controls tile which shows and hides the game controls mapping
  // hint.
  game_controls_tile_ = container->AddChildView(CreateFeatureTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnGameControlsTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, FeatureTile::TileType::kCompact,
      VIEW_ID_GD_CONTROLS_TILE, kGdGameControlsIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_CONTROLS_TILE_BUTTON_TITLE),
      /*sub_label=*/std::nullopt));

  UpdateGameControlsTile();
}

void GameDashboardMainMenuView::MaybeAddGameControlsDetailsRow(
    views::View* container) {
  auto flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  if (!flags) {
    return;
  }

  DCHECK(game_controls_tile_);

  CacheAppName();

  game_controls_details_ =
      container->AddChildView(std::make_unique<FeatureDetailsRow>(
          base::BindRepeating(
              &GameDashboardMainMenuView::OnGameControlsDetailsPressed,
              base::Unretained(this)),
          RoundedContainer::Behavior::kNotRounded,
          /*default_drill_in_arrow=*/false,
          /*icon=*/kGdGameControlsIcon, /*title=*/
          l10n_util::GetStringUTF16(
              IDS_ASH_GAME_DASHBOARD_CONTROLS_TILE_BUTTON_TITLE)));
  game_controls_details_->SetID(VIEW_ID_GD_CONTROLS_DETAILS_ROW);

  if (game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEmpty)) {
    game_controls_details_->SetSubtitle(
        l10n_util::GetStringUTF16(IDS_ASH_GAME_DASHBOARD_GC_SET_UP_SUB_TITLE));

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
  } else {
    const bool is_game_controls_enabled =
        game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kEnabled);
    UpdateGameControlsDetailsSubtitle(
        /*is_game_controls_enabled=*/is_game_controls_enabled);

    // Add toggle button and arrow icon for non-empty state.
    auto* edit_container = game_controls_details_->AddCustomizedTailView(
        std::make_unique<views::View>());
    edit_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        /*inside_border_insets=*/gfx::Insets(),
        /*between_child_spacing=*/18));
    edit_container->SetProperty(views::kMarginsKey,
                                gfx::Insets::TLBR(0, 8, 0, 0));

    // Add switch_button to enable or disable game controls.
    game_controls_feature_switch_ =
        edit_container->AddChildView(std::make_unique<Switch>(
            base::BindRepeating(&GameDashboardMainMenuView::
                                    OnGameControlsFeatureSwitchButtonPressed,
                                base::Unretained(this))));
    // TODO(b/279117180): Update the accessibility name.
    game_controls_feature_switch_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
    game_controls_feature_switch_->SetProperty(views::kMarginsKey,
                                               gfx::Insets::TLBR(0, 0, 0, 18));
    game_controls_feature_switch_->SetIsOn(is_game_controls_enabled);
    // Add arrow icon.
    edit_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            kQuickSettingsRightArrowIcon, cros_tokens::kCrosSysOnSurface)));
  }
}

void GameDashboardMainMenuView::MaybeAddScreenSizeSettingsRow(
    views::View* container) {
  aura::Window* game_window = context_->game_window();
  if (!IsArcWindow(game_window)) {
    return;
  }

  const auto resize_mode = compat_mode_util::PredictCurrentMode(game_window);
  auto* screen_size_row = container->AddChildView(CreateFeatureTile(
      base::BindRepeating(
          &GameDashboardMainMenuView::OnScreenSizeSettingsButtonPressed,
          base::Unretained(this)),
      /*is_togglable=*/false, FeatureTile::TileType::kPrimary,
      VIEW_ID_GD_SCREEN_SIZE_TILE,
      /*icon=*/compat_mode_util::GetIcon(resize_mode),
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_SCREEN_SIZE_SETTINGS_TITLE),
      /*sub_label=*/compat_mode_util::GetText(resize_mode)));
  // TODO(b/303351905): Investigate why drill in arrow isn't placed in correct
  // location.
  screen_size_row->CreateDecorativeDrillInArrow();
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
  feedback_button->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kBackgroundRadius));
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

  if (is_visible) {
    MaybeDecorateSetupButton();
  }
}

void GameDashboardMainMenuView::UpdateRecordGameTile(
    bool is_recording_game_window) {
  if (!record_game_tile_) {
    return;
  }

  record_game_tile_->SetEnabled(
      is_recording_game_window ||
      !CaptureModeController::Get()->is_recording_in_progress());

  record_game_tile_->SetVectorIcon(is_recording_game_window
                                       ? kCaptureModeCircleStopIcon
                                       : kGdRecordGameIcon);
  record_game_tile_->SetLabel(l10n_util::GetStringUTF16(
      is_recording_game_window
          ? IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_BUTTON_RECORDING_TITLE
          : IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_BUTTON_TITLE));
  if (is_recording_game_window) {
    record_game_tile_->SetSubLabel(context_->recording_duration());
  }
  record_game_tile_->SetSubLabelVisibility(is_recording_game_window);
  record_game_tile_->SetToggled(is_recording_game_window);
}

void GameDashboardMainMenuView::MaybeDecorateSetupButton() {
  if (!game_controls_setup_button_) {
    return;
  }
  PerformPulseAnimationForSetupButton(/*pulse_count=*/0);
  ShowNudgeForSetupButton();
}

void GameDashboardMainMenuView::PerformPulseAnimationForSetupButton(
    int pulse_count) {
  DCHECK(game_controls_setup_button_);

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
    gc_setup_button_pulse_layer_->SetColor(widget->GetColorProvider()->GetColor(
        cros_tokens::kCrosSysHighlightText));
  }

  DCHECK(gc_setup_button_pulse_layer_);

  // Initial setup button bounds in its widget coordinate.
  const auto setup_bounds = game_controls_setup_button_->ConvertRectToWidget(
      game_controls_setup_button_->bounds());

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
          &GameDashboardMainMenuView::PerformPulseAnimationForSetupButton,
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

void GameDashboardMainMenuView::ShowNudgeForSetupButton() {
  DCHECK(game_controls_setup_button_);

  // TODO(b/274690042): Replace it with localized strings.
  auto nudge_data = AnchoredNudgeData(
      kSetupNudgeId, ash::NudgeCatalogName::kGameDashboardControlsNudge,
      u"Set up to play with your keyboard", game_controls_details_);
  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_GAME_DASHBOARD_CONTROLS_SETUP_NUDGE);
  // TODO(b/274690042): Replace it with localized strings.
  nudge_data.title_text = u"This game uses your touchscreen";
  nudge_data.arrow = views::BubbleBorder::LEFT_CENTER;

  Shell::Get()->anchored_nudge_manager()->Show(nudge_data);
}

BEGIN_METADATA(GameDashboardMainMenuView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash
