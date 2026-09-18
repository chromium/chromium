// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {
const int kBubbleRowIconSize = 16;
const int kRedirectIconSize = 20;
// Size of the circle highlight around the redirect icon.
const int kRedirectIconRowHeight = 28;
const int kRowIconTopMargin = 6;
const int kLabelsContainerTopMargin = 4;
const int kLayoutInteriorMarginTop = 4;
const int kLayoutInteriorMarginRight = 8;

const gfx::VectorIcon& GetRowIcon(actor::ActorTask::State state) {
  if (glic::GlicActorTaskIconManager::RequiresAttention(state)) {
    return features::IsRoundedIconsEnabled() ? kHourglassIcon
                                             : kHourglassOldIcon;
  }
  if (state == actor::ActorTask::State::kFinished) {
    return features::IsRoundedIconsEnabled() ? kTaskSparkIcon
                                             : kTaskSparkOldIcon;
  }
  return kCursorSparkIcon;
}

ui::ColorId GetRowColor(bool is_enabled,
                        bool requires_processing,
                        bool needs_review) {
  if (!is_enabled) {
    return ui::kColorSysStateDisabled;
  }
  if (requires_processing && needs_review) {
    return ui::kColorSysPrimary;
  }
  return ui::kColorMenuIcon;
}

}  // namespace

ActorTaskListBubbleRowButton::ActorTaskListBubbleRowButton(
    views::Button::PressedCallback on_row_clicked,
    const actor::ui::ActorTaskRowData& row_data)
    : has_tab_(row_data.has_tab) {
  SetCallback(std::move(on_row_clicked));
  SetNotifyEnterExitOnChild(true);

  auto insets = ChromeLayoutProvider::Get()->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);
  const int horizontal_spacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL) /
      2;

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      // Top and right interior margins are adjusted to accommodate the redirect
      // icon's built-in margins for the hover highlight. This also requires the
      // row icon and labels container to slightly adjust their margins as well.
      .SetInteriorMargin(gfx::Insets::TLBR(
          insets.top() - kLayoutInteriorMarginTop, insets.left(),
          insets.bottom(), insets.right() - kLayoutInteriorMarginRight));

  const ui::ColorId row_color = GetRowColor(
      row_data.is_enabled, row_data.requires_processing, row_data.needs_review);

  row_icon_ = AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GetRowIcon(row_data.state), row_color, kBubbleRowIconSize)));
  row_icon_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kRowIconTopMargin, horizontal_spacing, 0,
                        horizontal_spacing));

  auto* labels_container =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  labels_container->SetOrientation(views::LayoutOrientation::kVertical);
  labels_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  labels_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kLabelsContainerTopMargin, horizontal_spacing, 0,
                        horizontal_spacing));

  title_ = labels_container->AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(row_data.title)));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  title_->SetSubpixelRenderingEnabled(false);

  subtitle_ = labels_container->AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(row_data.subtitle)));
  subtitle_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  subtitle_->SetTextStyle(views::style::STYLE_BODY_5);
  subtitle_->SetEnabledColor(row_color);
  subtitle_->SetSubpixelRenderingEnabled(false);

  redirect_icon_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&ActorTaskListBubbleRowButton::OnRedirectIconPressed,
                          base::Unretained(this)),
      features::IsRoundedIconsEnabled() ? vector_icons::kOpenInNewFlippableIcon
                                        : vector_icons::kLaunchOldIcon,
      kRedirectIconSize, ui::kColorMenuIcon, ui::kColorMenuIcon,
      ui::kColorMenuIcon));
  redirect_icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TAB_SEARCH_A11Y_OPEN_TAB));

  // Set the preferred size on the button directly to accommodate the circle
  // highlight
  redirect_icon_->SetPreferredSize(
      gfx::Size(kRedirectIconRowHeight, kRedirectIconRowHeight));
  views::InstallCircleHighlightPathGenerator(redirect_icon_.get());
  redirect_icon_->SetVisible(false);
  redirect_icon_->SetProperty(views::kMarginsKey,
                              gfx::Insets::VH(0, horizontal_spacing));

  // Add the hover highlight for the button row.
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColor(kColorHoverButtonBackgroundHovered);
  views::InkDrop::Get(this)->SetVisibleOpacity(1.0f);
  views::InkDrop::Get(this)->SetHighlightOpacity(1.0f);
  views::InstallRectHighlightPathGenerator(this);

  UpdateAccessibleName();

  // Update UI for "Tab closed" row after its first appearance.
  if (!row_data.is_enabled) {
    SetEnabled(false);
    title_->SetEnabledColor(ui::kColorSysStateDisabled);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
  }
}

ActorTaskListBubbleRowButton::~ActorTaskListBubbleRowButton() = default;

void ActorTaskListBubbleRowButton::OnRedirectIconPressed(
    const ui::Event& event) {
  NotifyClick(event);
}

void ActorTaskListBubbleRowButton::OnMouseEntered(const ui::MouseEvent& event) {
  Button::OnMouseEntered(event);
  // If the tab is closed, we never want to render the redirect icon.
  if (!has_tab_) {
    return;
  }
  SetState(Button::STATE_HOVERED);
  redirect_icon_->SetVisible(true);
}

void ActorTaskListBubbleRowButton::OnMouseExited(const ui::MouseEvent& event) {
  Button::OnMouseExited(event);
  // If the tab is closed, we never want to render the redirect icon.
  if (!has_tab_) {
    return;
  }
  SetState(Button::STATE_NORMAL);
  redirect_icon_->SetVisible(false);
}

std::u16string_view ActorTaskListBubbleRowButton::GetSubtitleText() const {
  return subtitle_ ? subtitle_->GetText() : std::u16string_view();
}

std::u16string_view ActorTaskListBubbleRowButton::GetTitleText() const {
  return title_ ? title_->GetText() : std::u16string_view();
}

void ActorTaskListBubbleRowButton::UpdateAccessibleName() {
  std::u16string_view subtitle_text = GetSubtitleText();
  std::u16string_view title_text = GetTitleText();

  Button::GetViewAccessibility().SetName(
      subtitle_text.empty() ? std::u16string(title_text)
                            : base::StrCat({title_text, u", ", subtitle_text}));
}

BEGIN_METADATA(ActorTaskListBubbleRowButton)
END_METADATA
