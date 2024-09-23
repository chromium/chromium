// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_task_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/vector_icons.h"

namespace ash {
namespace {

constexpr int kIconSize = 20;
constexpr int kCircleRadius = 18;

constexpr int kBetweenChildPadding = 16;
constexpr auto kInteriorMarginClamshell = gfx::Insets::TLBR(7, 8, 7, 16);
constexpr auto kInteriorMarginTablet = gfx::Insets::TLBR(13, 16, 13, 20);

constexpr int kViewCornerRadiusClamshell = 8;
constexpr int kViewCornerRadiusTablet = 20;
constexpr int kTaskMinWidth = 204;
constexpr int kTaskMaxWidth = 264;

gfx::ImageSkia CreateIconWithCircleBackground(const gfx::ImageSkia& icon,
                                              SkColor color) {
  return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      kCircleRadius, color, icon);
}

int GetCornerRadius(bool tablet_mode) {
  return tablet_mode ? kViewCornerRadiusTablet : kViewCornerRadiusClamshell;
}

}  // namespace

ContinueTaskView::ContinueTaskView(AppListViewDelegate* view_delegate,
                                   bool tablet_mode)
    : view_delegate_(view_delegate) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetCallback(base::BindRepeating(&ContinueTaskView::OnButtonPressed,
                                  base::Unretained(this)));
  auto ink_drop_highlight_path =
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(), GetCornerRadius(tablet_mode));
  ink_drop_highlight_path->set_use_contents_bounds(true);
  ink_drop_highlight_path->set_use_mirrored_rect(true);
  views::HighlightPathGenerator::Install(this,
                                         std::move(ink_drop_highlight_path));
  SetInstallFocusRingOnFocus(true);

  const ui::ColorId focus_ring_color = cros_tokens::kCrosSysFocusRing;
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
  views::FocusRing::Get(this)->SetColorId(focus_ring_color);
  SetFocusPainter(nullptr);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnHover(false);
  SetHasInkDropActionOnClick(true);
  SetShowInkDropWhenHotTracked(false);

  StyleUtil::ConfigureInkDropAttributes(
      this, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity,
      tablet_mode ? cros_tokens::kCrosSysRippleNeutralOnSubtle
                  : cros_tokens::kCrosSysHoverOnSubtle);

  if (tablet_mode) {
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(GetCornerRadius(/*tablet_mode=*/true)));

    const ui::ColorId background_color =
        static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemBaseElevated);
    SetBackground(views::CreateThemedSolidBackground(background_color));
    SetBorder(std::make_unique<views::HighlightBorder>(
        GetCornerRadius(/*tablet_mode=*/true),
        views::HighlightBorder::Type::kHighlightBorderNoShadow));
  }

  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      tablet_mode ? kInteriorMarginTablet : kInteriorMarginClamshell,
      kBetweenChildPadding));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  icon_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);

  auto* label_container = AddChildView(std::make_unique<views::View>());
  label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  title_ = label_container->AddChildView(
      std::make_unique<views::Label>(std::u16string()));
  bubble_utils::ApplyStyle(title_, TypographyToken::kCrosButton1,
                           cros_tokens::kCrosSysOnSurface);
  title_->GetViewAccessibility().SetName(std::u16string());
  title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

  subtitle_ = label_container->AddChildView(
      std::make_unique<views::Label>(std::u16string()));
  bubble_utils::ApplyStyle(subtitle_, TypographyToken::kCrosAnnotation1,
                           cros_tokens::kCrosSysOnSurfaceVariant);
  subtitle_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  subtitle_->SetElideBehavior(gfx::ElideBehavior::ELIDE_MIDDLE);

  layout_manager->SetFlexForView(label_container, 1);

  UpdateResult();
  set_context_menu_controller(this);
}
ContinueTaskView::~ContinueTaskView() {}

void ContinueTaskView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateIcon();
}

gfx::Size ContinueTaskView::GetMaximumSize() const {
  return gfx::Size(kTaskMaxWidth,
                   GetLayoutManager()->GetPreferredSize(this).height());
}

gfx::Size ContinueTaskView::GetMinimumSize() const {
  return gfx::Size(kTaskMinWidth,
                   GetLayoutManager()->GetPreferredSize(this).height());
}

gfx::Size ContinueTaskView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetMinimumSize();
}

void ContinueTaskView::OnButtonPressed(const ui::Event& event) {
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::ACTION_TRIGGERED);
  OpenResult(event.flags());
}

void ContinueTaskView::UpdateIcon() {
  if (!result()) {
    icon_->SetImage(gfx::ImageSkia());
    return;
  }

  if (!GetWidget()) {
    return;
  }

  gfx::ImageSkia icon;

  if (!result()->icon().icon.IsEmpty()) {
    icon = result()->icon().icon.Rasterize(GetColorProvider());
  } else {
    icon = result()->chip_icon();
  }

  icon_->SetImage(CreateIconWithCircleBackground(
      icon.size() == GetIconSize()
          ? icon
          : gfx::ImageSkiaOperations::CreateResizedImage(
                icon, skia::ImageOperations::RESIZE_BEST, GetIconSize()),
      GetColorProvider()->GetColor(GetIconBackgroundColorId())));
}

ui::ColorId ContinueTaskView::GetIconBackgroundColorId() const {
  if (result()->result_type() == AppListSearchResultType::kZeroStateHelpApp) {
    return cros_tokens::kCrosSysPrimary;
  }

  return cros_tokens::kCrosSysSystemOnBase;
}

gfx::Size ContinueTaskView::GetIconSize() const {
  return gfx::Size(kIconSize, kIconSize);
}

void ContinueTaskView::OnMetadataChanged() {
  UpdateResult();
}

void ContinueTaskView::UpdateResult() {
  SetVisible(!!result());
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::HIDDEN);
  CloseContextMenu();
  UpdateIcon();

  if (!result()) {
    title_->SetText(std::u16string());
    subtitle_->SetText(std::u16string());
    GetViewAccessibility().SetName(
        std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    return;
  }

  title_->SetText(result()->title());
  subtitle_->SetText(result()->details());
  subtitle_->SetVisible(!result()->details().empty());

  GetViewAccessibility().SetName(result()->title() + u" " + result()->details(),
                                 ax::mojom::NameFrom::kAttribute);
}

void ContinueTaskView::OnResultDestroying() {
  SetResult(nullptr);
}

void ContinueTaskView::SetResult(SearchResult* result) {
  if (result_ == result)
    return;

  search_result_observation_.Reset();

  result_ = result;
  if (result_) {
    search_result_observation_.Observe(result_.get());
    UpdateResult();
  }
}

void ContinueTaskView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // May be null if the result got reset, and the task view is animating out.
  if (!result())
    return;

  int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                  views::MenuRunner::CONTEXT_MENU |
                  views::MenuRunner::FIXED_ANCHOR;

  context_menu_runner_ =
      std::make_unique<views::MenuRunner>(BuildMenuModel(), run_types);

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), nullptr /*button_controller*/,
      source->GetBoundsInScreen(), views::MenuAnchorPosition::kBubbleTopRight,
      source_type);
  views::InkDrop::Get(this)->GetInkDrop()->SnapToActivated();
}

void ContinueTaskView::ExecuteCommand(int command_id, int event_flags) {
  CloseContextMenu();
  switch (command_id) {
    case ContinueTaskCommandId::kOpenResult:
      OpenResult(event_flags);
      break;
    case ContinueTaskCommandId::kRemoveResult:
      RemoveResult();
      break;
    case ContinueTaskCommandId::kHideContinueSection:
      view_delegate_->SetHideContinueSection(true);
      break;
    default:
      NOTREACHED();
  }
}

ui::SimpleMenuModel* ContinueTaskView::BuildMenuModel() {
  DCHECK(result_);
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  context_menu_model_->AddItemWithIcon(
      ContinueTaskCommandId::kOpenResult,
      l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_CONTINUE_SECTION_CONTEXT_MENU_OPEN),
      ui::ImageModel::FromVectorIcon(kLaunchIcon,
                                     ui::kColorAshSystemUIMenuIcon));

  // We won't create the `Remove suggestion` option for admin templates.
  // Reference: b/273800982.
  if (result_->result_type() != AppListSearchResultType::kDesksAdminTemplate) {
    context_menu_model_->AddItemWithIcon(
        ContinueTaskCommandId::kRemoveResult,
        l10n_util::GetStringUTF16(
            IDS_ASH_LAUNCHER_CONTINUE_SECTION_CONTEXT_MENU_REMOVE),
        ui::ImageModel::FromVectorIcon(kRemoveOutlineIcon,
                                       ui::kColorAshSystemUIMenuIcon));
  }

  if (Shell::Get()->IsInTabletMode()) {
    context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_model_->AddItemWithIcon(
        ContinueTaskCommandId::kHideContinueSection,
        l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_HIDE_CONTINUE_SECTION),
        ui::ImageModel::FromVectorIcon(kLauncherHideContinueSectionIcon,
                                       ui::kColorAshSystemUIMenuIcon));
  }
  return context_menu_model_.get();
}

void ContinueTaskView::MenuClosed(ui::SimpleMenuModel* menu) {
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::HIDDEN);
}

void ContinueTaskView::OpenResult(int event_flags) {
  // May be null if the result got reset, and the task view is animating out.
  if (!result())
    return;

  view_delegate_->OpenSearchResult(
      result()->id(), event_flags,
      AppListLaunchedFrom::kLaunchedFromContinueTask,
      AppListLaunchType::kSearchResult, index_in_container(),
      false /* launch_as_default */);
}

ContinueTaskView::TaskResultType ContinueTaskView::GetTaskResultType() {
  switch (result()->result_type()) {
    case AppListSearchResultType::kZeroStateFile:
      return TaskResultType::kLocalFile;
    case AppListSearchResultType::kZeroStateDrive:
      return TaskResultType::kDriveFile;
    default:
      NOTREACHED();
  }
}

void ContinueTaskView::RemoveResult() {
  // May be null if the result got reset, and the task view is animating out.
  if (!result())
    return;

  LogMetricsOnResultRemoved();
  view_delegate_->InvokeSearchResultAction(result()->id(),
                                           SearchResultActionType::kRemove);
}

bool ContinueTaskView::IsMenuShowing() const {
  return context_menu_runner_ && context_menu_runner_->IsRunning();
}

void ContinueTaskView::CloseContextMenu() {
  if (!IsMenuShowing())
    return;
  context_menu_runner_->Cancel();
}

void ContinueTaskView::LogMetricsOnResultRemoved() {
  RecordCumulativeContinueSectionResultRemovedNumber();

  base::UmaHistogramEnumeration("Apps.AppList.Search.ContinueResultRemoved",
                                GetTaskResultType(), TaskResultType::kMaxValue);
}

BEGIN_METADATA(ContinueTaskView)
END_METADATA

}  // namespace ash
