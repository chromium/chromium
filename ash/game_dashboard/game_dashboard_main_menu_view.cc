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
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
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
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

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
constexpr gfx::RoundedCornersF kGCDetailRowCorners =
    gfx::RoundedCornersF(/*upper_left=*/kDetailRowCornerRadius,
                         /*upper_right=*/kDetailRowCornerRadius,
                         /*lower_right=*/2.0f,
                         /*lower_left=*/2.0f);
constexpr gfx::RoundedCornersF kScreenSizeRowCorners =
    gfx::RoundedCornersF(/*upper_left=*/2.0f,
                         /*upper_right=*/2.0f,
                         /*lower_right=*/kDetailRowCornerRadius,
                         /*lower_left=*/kDetailRowCornerRadius);

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
              : cros_tokens::kCrosSysDisabledContainer,
      corners));

  // Set up highlight ink drop and focus ring.
  views::HighlightPathGenerator::Install(
      button, std::make_unique<views::RoundRectHighlightPathGenerator>(
                  gfx::Insets(), corners));
  StyleUtil::SetUpInkDropForButton(button, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/true);
  auto* focus_ring = views::FocusRing::Get(button);
  focus_ring->SetHaloInset(-4);
  focus_ring->SetHaloThickness(2);

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
                const std::u16string& title) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    // Add icon.
    auto* icon_container = AddChildView(std::make_unique<views::View>());
    icon_container->SetLayoutManager(std::make_unique<views::FillLayout>());
    icon_container->SetBackground(views::CreateThemedRoundedRectBackground(
        is_enabled ? cros_tokens::kCrosSysSystemOnBase
                   : cros_tokens::kCrosSysDisabledContainer,
        /*radius=*/12.0f));
    icon_container->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(6, 6)));
    icon_container->SetProperty(views::kMarginsKey,
                                gfx::Insets::TLBR(0, 0, 0, 16));
    icon_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            icon,
            is_enabled ? cros_tokens::kCrosSysOnSurface
                       : cros_tokens::kCrosSysDisabled,
            /*icon_size=*/20)));

    // Add title and sub-title.
    auto* tag_container =
        AddChildView(std::make_unique<views::BoxLayoutView>());
    tag_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
    tag_container->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    // Flex `tag_container` to fill empty space.
    layout->SetFlexForView(tag_container, /*flex=*/1);

    // Add title.
    auto* feature_title =
        tag_container->AddChildView(std::make_unique<views::Label>(title));
    feature_title->SetAutoColorReadabilityEnabled(false);
    feature_title->SetEnabledColorId(is_enabled
                                         ? cros_tokens::kCrosSysOnSurface
                                         : cros_tokens::kCrosSysDisabled);
    feature_title->SetFontList(
        TypographyProvider::Get()->ResolveTypographyToken(
            TypographyToken::kCrosTitle2));
    feature_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    feature_title->SetMultiLine(true);
    // Add sub-title.
    sub_title_ = tag_container->AddChildView(bubble_utils::CreateLabel(
        TypographyToken::kCrosAnnotation2, u"",
        is_enabled ? cros_tokens::kCrosSysOnSurfaceVariant
                   : cros_tokens::kCrosSysDisabled));
    sub_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    sub_title_->SetMultiLine(true);
  }

  FeatureHeader(const FeatureHeader&) = delete;
  FeatureHeader& operator=(const FeatureHeader) = delete;
  ~FeatureHeader() override = default;

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
  raw_ptr<views::Label> sub_title_ = nullptr;
};

BEGIN_METADATA(FeatureHeader)
END_METADATA

// -----------------------------------------------------------------------------
// ScreenSizeRow:

// ScreenSizeRow includes `FeatureHeader` and right arrow icon.
// +------------------------------------------------+
// | |feature header|                           |>| |
// +------------------------------------------------+
class ScreenSizeRow : public views::Button {
  METADATA_HEADER(ScreenSizeRow, views::Button)

 public:
  ScreenSizeRow(PressedCallback callback,
                ResizeCompatMode resize_mode,
                ArcResizeLockType resize_lock_type)
      : views::Button(std::move(callback)) {
    SetID(VIEW_ID_GD_SCREEN_SIZE_TILE);

    bool enabled = false;
    int tooltip = 0;
    switch (resize_lock_type) {
      case ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE:
      case ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE:
        enabled = true;
        break;
      case ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE:
        enabled = false;
        tooltip =
            IDS_ASH_ARC_APP_COMPAT_DISABLED_COMPAT_MODE_BUTTON_TOOLTIP_PHONE;
        break;
      case ArcResizeLockType::NONE:
        enabled = false;
        break;
    }

    const std::u16string title = l10n_util::GetStringUTF16(
        IDS_ASH_GAME_DASHBOARD_SCREEN_SIZE_SETTINGS_TITLE);
    SetAccessibleName(title);
    SetTooltipText(tooltip ? l10n_util::GetStringUTF16(tooltip) : title);

    auto* layout =
        ConfigureFeatureRowLayout(this, kScreenSizeRowCorners, enabled);
    // Add header.
    auto* header = AddChildView(std::make_unique<FeatureHeader>(
        enabled, compat_mode_util::GetIcon(resize_mode), title));
    layout->SetFlexForView(header, /*flex=*/1);
    header->UpdateSubtitle(compat_mode_util::GetText(resize_mode));
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
};

BEGIN_METADATA(ScreenSizeRow)
END_METADATA

}  // namespace

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
  explicit GameControlsDetailsRow(GameDashboardMainMenuView* main_menu)
      : views::Button(
            base::BindRepeating(&GameControlsDetailsRow::OnButtonPressed,
                                base::Unretained(this))),
        main_menu_(main_menu) {
    CacheAppName();
    SetID(VIEW_ID_GD_CONTROLS_DETAILS_ROW);

    const auto flags =
        game_dashboard_utils::GetGameControlsFlag(GetGameWindow());
    CHECK(flags);

    const auto title = l10n_util::GetStringUTF16(
        IDS_ASH_GAME_DASHBOARD_CONTROLS_TILE_BUTTON_TITLE);
    SetAccessibleName(title);
    SetTooltipText(title);

    const bool is_available = game_dashboard_utils::IsFlagSet(
        *flags, ArcGameControlsFlag::kAvailable);
    auto* layout =
        ConfigureFeatureRowLayout(this, kGCDetailRowCorners, is_available);

    // Add header.
    header_ = AddChildView(std::make_unique<FeatureHeader>(
        /*is_enabled=*/is_available, kGdGameControlsIcon, title));
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
        // TODO(b/274690042): Replace it with localized strings.
        setup_button_->SetTooltipText(
            u"This game does not support Game controls");
      }
    } else {
      const bool is_feature_enabled = IsGameControlsFeatureEnabled(*flags);
      UpdateSubtitle(/*is_game_controls_enabled=*/is_feature_enabled);
      // Add switch_button to enable or disable game controls.
      feature_switch_ =
          AddChildView(std::make_unique<Switch>(base::BindRepeating(
              &GameControlsDetailsRow::OnFeatureSwitchButtonPressed,
              base::Unretained(this))));
      // TODO(b/279117180): Update the accessibility name.
      feature_switch_->SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
      feature_switch_->SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(0, 8, 0, 18));
      feature_switch_->SetIsOn(is_feature_enabled);
      // Add arrow icon.
      AddChildView(
          std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
              kQuickSettingsRightArrowIcon, cros_tokens::kCrosSysOnSurface)));
    }
  }

  GameControlsDetailsRow(const GameControlsDetailsRow&) = delete;
  GameControlsDetailsRow& operator=(const GameControlsDetailsRow) = delete;
  ~GameControlsDetailsRow() override = default;

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
    const bool is_toggled = feature_switch_->GetIsOn();
    UpdateSubtitle(/*is_game_controls_enabled=*/is_toggled);

    auto* game_window = GetGameWindow();
    game_window->SetProperty(
        kArcGameControlsFlagsKey,
        game_dashboard_utils::UpdateFlag(
            game_window->GetProperty(kArcGameControlsFlagsKey),
            static_cast<ArcGameControlsFlag>(
                /*enable_flag=*/ArcGameControlsFlag::kEnabled |
                ArcGameControlsFlag::kHint),
            is_toggled));

    main_menu_->UpdateGameControlsTile();
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
    main_menu_->context_->CloseMainMenu();

    auto* game_window = GetGameWindow();
    game_window->SetProperty(
        kArcGameControlsFlagsKey,
        game_dashboard_utils::UpdateFlag(
            game_window->GetProperty(kArcGameControlsFlagsKey),
            ArcGameControlsFlag::kEdit, /*enable_flag=*/true));
  }

  aura::Window* GetGameWindow() { return main_menu_->context_->game_window(); }

  const raw_ptr<GameDashboardMainMenuView> main_menu_;

  raw_ptr<FeatureHeader> header_ = nullptr;
  raw_ptr<PillButton> setup_button_ = nullptr;
  raw_ptr<Switch> feature_switch_ = nullptr;

  // App name from the app where this view is anchored.
  std::string app_name_;
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
      gfx::Insets::VH(kPaddingHeight, kPaddingWidth),
      /*between_child_spacing=*/16));

  AddShortcutTilesRow();
  MaybeAddArcFeatureRows();
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

void GameDashboardMainMenuView::UpdateGameControlsTile() {
  DCHECK(game_controls_tile_);

  const auto flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  CHECK(flags);

  game_dashboard_utils::UpdateGameControlsHintButton(game_controls_tile_,
                                                     *flags);
}

void GameDashboardMainMenuView::OnScreenSizeSettingsButtonPressed() {
  context_->CloseMainMenu();
  GameDashboardController::Get()->ShowResizeToggleMenu(context_->game_window());
}

void GameDashboardMainMenuView::OnFeedbackButtonPressed() {
  Shell::Get()->shell_delegate()->OpenFeedbackDialog(
      ShellDelegate::FeedbackSource::kGameDashboard,
      /*description_template=*/"#GameDashboard\n\n");
}

void GameDashboardMainMenuView::OnHelpButtonPressed() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kHelpUrl), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
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

void GameDashboardMainMenuView::MaybeAddArcFeatureRows() {
  if (!IsArcWindow(context_->game_window())) {
    return;
  }

  auto* feature_details_container =
      AddChildView(std::make_unique<views::View>());
  feature_details_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          /*inside_border_insets=*/gfx::Insets(),
          /*between_child_spacing=*/2));

  AddGameControlsDetailsRow(feature_details_container);
  AddScreenSizeSettingsRow(feature_details_container);
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

  // Call `SetSubLabelVisibility` after the sub-label is set.
  game_controls_tile_->SetSubLabelVisibility(true);
}

void GameDashboardMainMenuView::AddGameControlsDetailsRow(
    views::View* container) {
  DCHECK(IsArcWindow(context_->game_window()));
  game_controls_details_ =
      container->AddChildView(std::make_unique<GameControlsDetailsRow>(this));
}

void GameDashboardMainMenuView::AddScreenSizeSettingsRow(
    views::View* container) {
  aura::Window* game_window = context_->game_window();
  DCHECK(IsArcWindow(game_window));
  container->AddChildView(std::make_unique<ScreenSizeRow>(
      base::BindRepeating(
          &GameDashboardMainMenuView::OnScreenSizeSettingsButtonPressed,
          base::Unretained(this)),
      /*resize_mode=*/compat_mode_util::PredictCurrentMode(game_window),
      /*resize_lock_type=*/game_window->GetProperty(kArcResizeLockTypeKey)));
}

void GameDashboardMainMenuView::AddUtilityClusterRow() {
  auto* container = AddChildView(std::make_unique<views::View>());
  auto* layout = container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      /*inside_border_insets=*/gfx::Insets(),
      /*between_child_spacing=*/16));

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

  if (is_visible) {
    MaybeDecorateSetupButton(
        game_dashboard_utils::IsFlagSet(*flags, ArcGameControlsFlag::kO4C));
  }
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
}

void GameDashboardMainMenuView::MaybeDecorateSetupButton(bool is_o4c) {
  if (!GetGameControlsSetupButton() || is_o4c) {
    return;
  }
  ShowNudgeForSetupButton();
  PerformPulseAnimationForSetupButton(/*pulse_count=*/0);
}

void GameDashboardMainMenuView::PerformPulseAnimationForSetupButton(
    int pulse_count) {
  auto* setup_button = GetGameControlsSetupButton();
  DCHECK(setup_button);

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
  const auto setup_bounds =
      setup_button->ConvertRectToWidget(gfx::Rect(setup_button->size()));

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
  DCHECK(GetGameControlsSetupButton());

  auto nudge_data = AnchoredNudgeData(
      kSetupNudgeId, NudgeCatalogName::kGameDashboardControlsNudge,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_GC_KEYBOARD_SETUP_NUDGE_SUB_TITLE),
      game_controls_details_);
  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_GAME_DASHBOARD_CONTROLS_SETUP_NUDGE);
  nudge_data.title_text = l10n_util::GetStringUTF16(
      IDS_ASH_GAME_DASHBOARD_GC_KEYBOARD_SETUP_NUDGE_TITLE);
  nudge_data.arrow = views::BubbleBorder::LEFT_CENTER;
  nudge_data.background_color_id = cros_tokens::kCrosSysBaseHighlight;
  nudge_data.image_background_color_id = cros_tokens::kCrosSysOnBaseHighlight;
  nudge_data.duration = NudgeDuration::kMediumDuration;

  Shell::Get()->anchored_nudge_manager()->Show(nudge_data);
}

PillButton* GameDashboardMainMenuView::GetGameControlsSetupButton() {
  return game_controls_details_ ? game_controls_details_->setup_button()
                                : nullptr;
}

Switch* GameDashboardMainMenuView::GetGameControlsFeatureSwith() {
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

void GameDashboardMainMenuView::OnThemeChanged() {
  views::View::OnThemeChanged();
  set_color(GetColorProvider()->GetColor(
      cros_tokens::kCrosSysSystemBaseElevatedOpaque));
}

BEGIN_METADATA(GameDashboardMainMenuView)
END_METADATA

}  // namespace ash
