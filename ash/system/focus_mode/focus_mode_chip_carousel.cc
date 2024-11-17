// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_chip_carousel.h"

#include "ash/api/tasks/tasks_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/containers/adapters.h"
#include "base/i18n/rtl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/gesture_event_details.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr auto kCarouselInsets = gfx::Insets::TLBR(16, 0, 0, 0);
constexpr int kChipSpaceBetween = 8;
constexpr int kChipHeight = 32;
constexpr int kChipMaxWidth = 216;
constexpr auto kChipInsets = gfx::Insets::VH(0, 12);
constexpr int kChipCornerRadius = 16;
constexpr size_t kMaxTasks = 5;
constexpr int kChevronSize = 16;
constexpr int kOverflowButtonWidth = 28;
constexpr float kGradientWidth = 16;

// How far from the left the scrolled-to chip should be to ensure some of the
// previous chip is visible.
constexpr int kFirstChipOffsetX =
    kOverflowButtonWidth + kGradientWidth + kChipSpaceBetween;

void SetupChip(views::LabelButton* chip, bool first) {
  chip->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  chip->SetBorder(views::CreatePaddedBorder(
      views::CreateThemedRoundedRectBorder(1, kChipHeight,
                                           cros_tokens::kCrosSysSeparator),
      kChipInsets));
  // Add a border to space out chips on all chips but the first.
  chip->SetProperty(views::kMarginsKey,
                    gfx::Insets::TLBR(0, first ? 0 : kChipSpaceBetween, 0, 0));
  chip->SetLabelStyle(views::style::STYLE_BODY_3_MEDIUM);
  chip->SetMinSize(gfx::Size(0, kChipHeight));
  chip->SetMaxSize(gfx::Size(kChipMaxWidth, kChipHeight));
  views::FocusRing::Get(chip)->SetColorId(cros_tokens::kCrosSysFocusRing);
  // Remove the padding between the focus ring and the `chip`.
  views::InstallRoundRectHighlightPathGenerator(chip, gfx::Insets(4),
                                                kChipCornerRadius);
  chip->SetNotifyEnterExitOnChild(true);
  chip->SetTooltipText(chip->GetText());

  views::ViewAccessibility& view_accessibility = chip->GetViewAccessibility();
  view_accessibility.SetName(chip->GetText());
  // Set the list item role with a description to let the users know that they
  // can press this item as a button.
  view_accessibility.SetRole(
      ax::mojom::Role::kListItem,
      l10n_util::GetStringUTF16(IDS_ASH_A11Y_ROLE_BUTTON));
}

void SetupOverflowIcon(views::ImageButton* overflow_icon, bool left) {
  overflow_icon->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(left ? kCaretLeftIcon : kCaretRightIcon,
                                     cros_tokens::kCrosSysOnSurface,
                                     kChevronSize));
  overflow_icon->SetTooltipText(left ? u"Scroll Left" : u"Scroll Right");
  overflow_icon->SetPreferredSize(gfx::Size(kOverflowButtonWidth, kChipHeight));
  overflow_icon->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  overflow_icon->SetImageHorizontalAlignment(
      left ? views::ImageButton::ALIGN_LEFT : views::ImageButton::ALIGN_RIGHT);
  overflow_icon->SetPaintToLayer();
  overflow_icon->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  overflow_icon->layer()->SetFillsBoundsOpaquely(false);
}

bool IsVerticalScrollGesture(const ui::Event& event) {
  if (!event.IsGestureEvent()) {
    return false;
  }

  auto is_vertical = [](float x_offset, float y_offset) -> bool {
    return std::fabs(x_offset) <= std::fabs(y_offset);
  };

  const auto& details = event.AsGestureEvent()->details();
  return (event.type() == ui::EventType::kGestureScrollUpdate &&
          is_vertical(details.scroll_x(), details.scroll_y())) ||
         (event.type() == ui::EventType::kGestureScrollBegin &&
          is_vertical(details.scroll_x_hint(), details.scroll_y_hint())) ||
         (event.type() == ui::EventType::kScrollFlingStart &&
          is_vertical(details.velocity_x(), details.velocity_y()));
}

class ChipCarouselScrollView : public views::ScrollView {
  METADATA_HEADER(ChipCarouselScrollView, views::ScrollView)

 public:
  explicit ChipCarouselScrollView(ScrollWithLayers scroll_with_layers)
      : views::ScrollView(scroll_with_layers) {}

  // views::ScrollView:
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override {
    // We want this scroll view to only handle the horizontal scroll events on
    // it; if the user scrolls on it vertically, we want the outer scroll view
    // to handle it.
    return horizontal_scroll_bar()->OnScroll(event.x_offset(), 0);
  }

  // views::View:
  bool CanAcceptEvent(const ui::Event& event) override {
    // The vertical scroll gesture event should be handled by the outer scroll
    // view instead of this view.
    return views::ScrollView::CanAcceptEvent(event) &&
           !IsVerticalScrollGesture(event);
  }
};
BEGIN_METADATA(ChipCarouselScrollView)
END_METADATA

}  // namespace

// `on_chip_pressed` will be called when a task chip is clicked, containing a
// task.
FocusModeChipCarousel::FocusModeChipCarousel(
    ChipPressedCallback on_chip_pressed)
    : on_chip_pressed_(std::move(on_chip_pressed)) {
  SetBorder(views::CreateEmptyBorder(kCarouselInsets));
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetNotifyEnterExitOnChild(true);

  scroll_view_ = AddChildView(std::make_unique<ChipCarouselScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetPaintToLayer();
  scroll_view_->SetBackgroundColor(std::nullopt);

  scroll_contents_ =
      scroll_view_->SetContents(std::make_unique<views::FlexLayoutView>());
  scroll_contents_->SetOrientation(views::LayoutOrientation::kHorizontal);
  scroll_contents_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));

  views::ViewAccessibility& scroll_contents_view_accessibility =
      scroll_contents_->GetViewAccessibility();
  scroll_contents_view_accessibility.SetRole(ax::mojom::Role::kList);
  scroll_contents_view_accessibility.SetName(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_SUGGESTED_TASKS));

  left_overflow_icon_ = AddChildView(std::make_unique<views::ImageButton>(
      base::BindRepeating(&FocusModeChipCarousel::OnChevronPressed,
                          base::Unretained(this), /*left=*/true)));
  SetupOverflowIcon(left_overflow_icon_, /*left=*/true);
  right_overflow_icon_ = AddChildView(std::make_unique<views::ImageButton>(
      base::BindRepeating(&FocusModeChipCarousel::OnChevronPressed,
                          base::Unretained(this), /*left=*/false)));
  SetupOverflowIcon(right_overflow_icon_, /*left=*/false);

  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
          &FocusModeChipCarousel::UpdateGradient, base::Unretained(this)));
  on_contents_scroll_ended_subscription_ =
      scroll_view_->AddContentsScrollEndedCallback(base::BindRepeating(
          &FocusModeChipCarousel::UpdateGradient, base::Unretained(this)));
}

FocusModeChipCarousel::~FocusModeChipCarousel() = default;

void FocusModeChipCarousel::Layout(PassKey) {
  if (!GetVisible()) {
    return;
  }

  LayoutSuperclass<views::View>(this);
  scroll_contents_->SizeToPreferredSize();

  const gfx::Rect contents_bounds = GetContentsBounds();
  const int x = contents_bounds.x();
  const int y = contents_bounds.y();
  const int h = contents_bounds.height();

  left_overflow_icon_->SetBoundsRect(gfx::Rect(x, y, kOverflowButtonWidth, h));
  right_overflow_icon_->SetBoundsRect(
      gfx::Rect(contents_bounds.right() - kOverflowButtonWidth, y,
                kOverflowButtonWidth, h));

  UpdateGradient();
}

void FocusModeChipCarousel::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateGradient();
}

void FocusModeChipCarousel::OnMouseExited(const ui::MouseEvent& event) {
  UpdateGradient();
}

void FocusModeChipCarousel::SetTasks(const std::vector<FocusModeTask>& tasks) {
  scroll_contents_->RemoveAllChildViews();
  if (tasks.empty()) {
    return;
  }

  // Populate a maximum of `kMaxTasks` tasks.
  const size_t num_tasks = std::min(tasks.size(), kMaxTasks);
  for (size_t i = 0; i < num_tasks; i++) {
    // Skip empty task.
    if (tasks[i].title.empty()) {
      continue;
    }
    views::LabelButton* chip =
        scroll_contents_->AddChildView(std::make_unique<views::LabelButton>(
            base::BindRepeating(on_chip_pressed_, tasks[i]),
            base::UTF8ToUTF16(tasks[i].title)));
    SetupChip(chip, /*first=*/(i == 0));
  }

  // After adding the child views to the contents of the scroll view, we need to
  // manually call the function to update the bounds, so that the horizontal
  // scroll bar can have a non-zero `max_pos_` to allow the chip carousel to
  // scroll horizontally. See b/346877741.
  scroll_view_->contents()->SizeToPreferredSize();

  // Scroll back to the beginning after repopulating the carousel.
  scroll_view_->ScrollToOffset(gfx::PointF(0, 0));
}

void FocusModeChipCarousel::UpdateGradient() {
  const gfx::Rect visible_rect = scroll_view_->GetVisibleRect();
  // Show the left gradient if the scroll view is not scrolled to the left.
  const bool show_left_gradient = visible_rect.x() > 0;
  // Show the right gradient if the scroll view is not scrolled to the right.
  const bool show_right_gradient =
      visible_rect.right() < scroll_view_->contents()->bounds().right();
  const bool hovered = IsMouseHovered();

  left_overflow_icon_->SetVisible(show_left_gradient && hovered);
  right_overflow_icon_->SetVisible(show_right_gradient && hovered);

  // If no gradient is needed, remove the gradient mask.
  if (scroll_view_->contents()->bounds().IsEmpty() ||
      scroll_view_->bounds().IsEmpty() ||
      (!show_left_gradient && !show_right_gradient)) {
    RemoveGradient();
    return;
  }

  // Horizontal linear gradient, from left to right.
  gfx::LinearGradient gradient_mask(/*angle=*/0);

  // We want a completely transparent section at the beginning for the chevron,
  // and then a gradient section. Only add extra space for the chevrons if the
  // carousel is hovered, otherwise the chevrons won't be shown.
  const float chevron_space = hovered ? kOverflowButtonWidth : 0;
  const float gradient_start_position =
      chevron_space / scroll_view_->bounds().width();
  const float gradient_end_position =
      (chevron_space + kGradientWidth) / scroll_view_->bounds().width();

  // Left fade in section. Gradients don't account for RTL like other `Views`
  // coordinates do, so we need to flip to account for RTL ourselves.
  if (base::i18n::IsRTL() ? show_right_gradient : show_left_gradient) {
    gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
    if (hovered) {
      gradient_mask.AddStep(gradient_start_position, 0);
    }
    gradient_mask.AddStep(gradient_end_position, 255);
  }

  // Right fade out section.
  if (base::i18n::IsRTL() ? show_left_gradient : show_right_gradient) {
    gradient_mask.AddStep(/*fraction=*/(1 - gradient_end_position),
                          /*alpha=*/255);
    if (hovered) {
      gradient_mask.AddStep((1 - gradient_start_position), 0);
    }
    gradient_mask.AddStep(1, 0);
  }

  if (scroll_view_->layer()->gradient_mask() != gradient_mask) {
    scroll_view_->layer()->SetGradientMask(gradient_mask);
  }
}

void FocusModeChipCarousel::RemoveGradient() {
  if (scroll_view_->layer()->HasGradientMask()) {
    scroll_view_->layer()->SetGradientMask(gfx::LinearGradient::GetEmpty());
  }
}

void FocusModeChipCarousel::OnChevronPressed(bool left) {
  const int align_point_x =
      scroll_view_->GetVisibleRect().x() + kFirstChipOffsetX;

  // Pressing the chevrons should position the next chip with some offset
  // `kFirstChipOffsetX` from the left so you can still see the previous and
  // next chips. When scrolling right, check from the start and scroll to the
  // first chip whose origin is past the desired position. When scrolling left,
  // start from the end and scroll to the first chip whose origin is to the left
  // of the desired position.
  View::Views children = scroll_contents_->GetChildrenInZOrder();
  for (size_t i = 0; i < children.size(); i++) {
    views::View* chip_view = children[left ? (children.size() - 1) - i : i];
    const int chip_left =
        views::View::ConvertRectToTarget(chip_view, scroll_contents_,
                                         chip_view->GetLocalBounds())
            .x();
    if (left ? chip_left < align_point_x : chip_left > align_point_x) {
      ScrollToChip(chip_view);
      return;
    }
  }

  // Pressing a chevron should always result in a scroll.
  NOTREACHED();
}

void FocusModeChipCarousel::ScrollToChip(views::View* chip) {
  const gfx::Rect viewport = scroll_view_->GetVisibleRect();
  const int chip_left = views::View::ConvertRectToTarget(chip, scroll_contents_,
                                                         chip->GetLocalBounds())
                            .x();
  const int scroll_offset = chip_left - viewport.x() - kFirstChipOffsetX;

  // Don't scroll past the end of `scroll_contents_`.
  int scroll_total;
  if (scroll_offset < 0) {
    // The scroll offset to scroll all the way to the left.
    const int min_scroll =
        scroll_view_->contents()->bounds().x() - viewport.x();

    scroll_total = std::max(scroll_offset, min_scroll);
  } else {
    // The scroll offset to scroll all the way to the right.
    const int max_scroll =
        scroll_view_->contents()->bounds().right() - viewport.right();

    scroll_total = std::min(scroll_offset, max_scroll);
  }

  scroll_view_->ScrollByOffset(gfx::PointF(scroll_total, 0));
  SchedulePaint();
}

bool FocusModeChipCarousel::HasTasks() const {
  return !scroll_contents_->GetChildrenInZOrder().empty();
}

int FocusModeChipCarousel::GetTaskCountForTesting() const {
  return scroll_contents_->GetChildrenInZOrder().size();
}

views::ScrollView* FocusModeChipCarousel::GetScrollViewForTesting() const {
  return views::AsViewClass<views::ScrollView>(scroll_view_);
}

BEGIN_METADATA(FocusModeChipCarousel)
END_METADATA

}  // namespace ash
