// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/pagination_view.h"

#include <optional>
#include <utility>

#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "base/i18n/number_formatting.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// The minimum number of pages to show the pagination view.
constexpr int kMinNumPages = 2;

// Attributes of arrow buttons.
constexpr int kArrowButtonIconSize = 20;
constexpr ui::ColorId kArrowButtonColorId = cros_tokens::kCrosSysSecondary;
constexpr int kArrowIndicatorSpacing = 2;

// Attributes of indicator.
constexpr int kIndicatorButtonSize = 20;
constexpr int kIndicatorRadius = 4;
constexpr int kIndicatorStrokeWidth = 1;
constexpr int kIndicatorSpacing = 2;
constexpr ui::ColorId kIndicatorColorId = cros_tokens::kCrosSysPrimary;
constexpr int kMaxNumVisibleIndicators = 5;

// Get the width of the indicator container.
int GetIndicatorContainerWidth(int total_pages) {
  if (total_pages < kMinNumPages) {
    return 0;
  }

  const int visible_num = std::min(total_pages, kMaxNumVisibleIndicators);
  return visible_num * kIndicatorButtonSize +
         (visible_num - 1) * kIndicatorSpacing;
}

// A structure holds the info needed by interpolation.
template <typename T>
struct InterpolationInterval {
  // The start time and value.
  double start_time;
  T start_value;
  // The end time and value.
  double end_time;
  T target_value;
};

// IndicatorButton:
// A button with a hollow circle in the center.
class IndicatorButton : public views::Button {
  METADATA_HEADER(IndicatorButton, views::Button)

 public:
  IndicatorButton(PressedCallback callback,
                  const std::u16string& accessible_name)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
    GetViewAccessibility().SetName(accessible_name);
  }

  IndicatorButton(const IndicatorButton&) = delete;
  IndicatorButton& operator=(const IndicatorButton&) = delete;
  ~IndicatorButton() override = default;

  // Gets the bounds of the circle in the center.
  gfx::Rect GetIndicatorBounds() const {
    gfx::Rect indicator_bounds = bounds();
    indicator_bounds.Inset(
        gfx::Insets(0.5 * kIndicatorButtonSize - kIndicatorRadius));
    return indicator_bounds;
  }

  // views::Button:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kIndicatorButtonSize, kIndicatorButtonSize);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetColorProvider()->GetColor(kIndicatorColorId));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kIndicatorStrokeWidth);
    // Do inner stroke.
    canvas->DrawCircle(GetLocalBounds().CenterPoint(),
                       kIndicatorRadius - 0.5f * kIndicatorStrokeWidth, flags);
  }
};

BEGIN_METADATA(IndicatorButton)
END_METADATA

}  // namespace

//------------------------------------------------------------------------------
// PaginationView::SelectorDotView:
// A solid circle that performs deformation with the pace of page transition.
class PaginationView::SelectorDotView : public views::View {
  METADATA_HEADER(SelectorDotView, views::View)

 public:
  using DeformInterval = InterpolationInterval<gfx::Rect>;

  SelectorDotView() {
    SetBackground(
        StyleUtil::CreateThemedFullyRoundedRectBackground(kIndicatorColorId));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    // Set selector dot ignored by layout since it will follow selected
    // indicator and deform on page transition.
    SetProperty(views::kViewIgnoredByLayoutKey, true);
  }

  SelectorDotView(const SelectorDotView&) = delete;
  SelectorDotView& operator=(const SelectorDotView&) = delete;
  ~SelectorDotView() override = default;

  // Adds a new deform interval.
  void AddDeformInterval(DeformInterval interval) {
    DCHECK_LT(interval.start_time, interval.end_time);
    deform_intervals_.push_back(interval);
    // Sort the intervals according to the start time in ascending order.
    std::sort(
        deform_intervals_.begin(), deform_intervals_.end(),
        [](const DeformInterval& interval_1, const DeformInterval& interval_2) {
          return interval_1.start_time < interval_2.start_time;
        });
  }

  // Performs deformation according to the given progress within deform
  // intervals.
  void Deform(double progress) {
    if (deform_intervals_.empty()) {
      return;
    }

    auto iter = std::find_if(deform_intervals_.begin(), deform_intervals_.end(),
                             [&](DeformInterval& interval) {
                               return interval.start_time <= progress &&
                                      interval.end_time >= progress;
                             });

    if (iter == deform_intervals_.end()) {
      return;
    }

    // Get intermediate bounds by interpolating the origin and target bounds.
    const gfx::Rect intermediate_bounds = gfx::Tween::RectValueBetween(
        (progress - iter->start_time) / (iter->end_time - iter->start_time),
        iter->start_value, iter->target_value);
    SetBoundsRect(intermediate_bounds);
  }

  void ResetDeform(bool canceled) {
    if (!deform_intervals_.empty()) {
      SetBoundsRect(canceled ? deform_intervals_.front().start_value
                             : deform_intervals_.back().target_value);
    }
    deform_intervals_.clear();
  }

  // Returns true if deformation is still in progress.
  bool DeformingInProgress() const { return !deform_intervals_.empty(); }

 private:
  std::vector<DeformInterval> deform_intervals_;
};

BEGIN_METADATA(PaginationView, SelectorDotView)
END_METADATA

//------------------------------------------------------------------------------
// PaginationView::IndicatorContainer:
// The container of indicators. If the indicator to be selected is not visible,
// the container will scroll with the pace of pagination transition.
class PaginationView::IndicatorContainer : public views::BoxLayoutView {
  METADATA_HEADER(IndicatorContainer, views::BoxLayoutView)

 public:
  explicit IndicatorContainer(views::BoxLayout::Orientation orientation) {
    SetOrientation(orientation);
    SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
    SetBetweenChildSpacing(kIndicatorSpacing);
  }

  IndicatorContainer(const IndicatorContainer&) = delete;
  IndicatorContainer& operator=(const IndicatorContainer&) = delete;
  ~IndicatorContainer() override = default;

  // Attaches an indicator to the end of container.
  void PushIndicator(PaginationModel* model) {
    const int page = buttons_.size();
    // Since the selector dot will also be added in the container, we should use
    // `AddChildViewAt` to ensure the indicator is in the expected position in
    // the child views.
    auto* indicator_button = AddChildViewAt(
        std::make_unique<IndicatorButton>(
            base::BindRepeating(
                [](PaginationModel* model, int page, const ui::Event& event) {
                  model->SelectPage(page, /*animate=*/true);
                },
                model, page),
            l10n_util::GetStringFUTF16(
                IDS_APP_LIST_PAGE_SWITCHER, base::FormatNumber(page + 1),
                base::FormatNumber(model->total_pages()))),
        page);
    buttons_.emplace_back(indicator_button);
  }

  // Discards the indicator at the end of the container.
  void PopIndicator() {
    DCHECK(buttons_.size());
    auto indicator_button = buttons_.back();
    buttons_.pop_back();
    RemoveChildViewT(std::exchange(indicator_button, nullptr));
  }

  // Gets indicator corresponding to the given page.
  IndicatorButton* GetIndicatorByPage(int page) {
    DCHECK_GE(page, 0);
    DCHECK_LT(page, static_cast<int>(buttons_.size()));
    return buttons_[page].get();
  }

  int GetNumberOfIndicators() const { return buttons_.size(); }

  // Sets up scrolling if an invisible page is selected.
  void StartScroll(int start_page, int target_page) {
    // Scroll the indicator container by the distance of a indicator button size
    // plus button spacing to reveal the next/previous indicator.
    // TODO(zxdan): settings bounds at each step will cause repainting which is
    // expensive. However, using transform sometimes makes the stroke of
    // indicator circle become thicker. Will investigate the cause latter.
    const bool forward = start_page < target_page;
    const int start_page_offset =
        forward ? kMaxNumVisibleIndicators - start_page - 1 : -start_page;
    const int target_page_offset =
        forward ? kMaxNumVisibleIndicators - target_page - 1 : -target_page;
    const int scroll_unit = kIndicatorButtonSize + kIndicatorSpacing;
    scroll_interval_ = {0.0, start_page_offset * scroll_unit, 1.0,
                        target_page_offset * scroll_unit};
  }

  // Scrolls the indicator container according to the given progress value.
  void Scroll(double progress) {
    if (!scroll_interval_) {
      return;
    }

    // Interpolate the scroll interval to get current container bounds.
    ScrollWithOffset(
        gfx::Tween::IntValueBetween(progress, scroll_interval_->start_value,
                                    scroll_interval_->target_value));
  }

  void ResetScroll(bool canceled) {
    if (scroll_interval_) {
      ScrollWithOffset(canceled ? scroll_interval_->start_value
                                : scroll_interval_->target_value);
    }
    scroll_interval_ = std::nullopt;
  }

  // Returns true if the scrolling is in progress.
  bool ScrollingInProgress() { return !!scroll_interval_; }

 private:
  // Scroll horizontally or vertically with given offset.
  void ScrollWithOffset(int offset) {
    if (GetOrientation() == views::BoxLayout::Orientation::kHorizontal) {
      SetX(offset);
    } else {
      SetY(offset);
    }
  }

  std::vector<raw_ptr<IndicatorButton>> buttons_;
  std::optional<InterpolationInterval<int>> scroll_interval_;
};

BEGIN_METADATA(PaginationView, IndicatorContainer)
END_METADATA

//------------------------------------------------------------------------------
// PaginationView:
PaginationView::PaginationView(PaginationModel* model, Orientation orientation)
    : model_(model),
      orientation_(orientation),
      indicator_scroll_view_(
          AddChildView(std::make_unique<views::ScrollView>())),
      indicator_container_(indicator_scroll_view_->SetContents(
          std::make_unique<IndicatorContainer>(
              orientation == Orientation::kHorizontal
                  ? views::BoxLayout::Orientation::kHorizontal
                  : views::BoxLayout::Orientation::kVertical))) {
  DCHECK(model_);
  model_observation_.Observe(model_.get());

  // Remove the default background color.
  indicator_scroll_view_->SetBackgroundColor(std::nullopt);

  // The scroll view does not accept any scroll event.
  indicator_scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  indicator_scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);

  if (model_->total_pages() >= kMinNumPages) {
    TotalPagesChanged(0, model_->total_pages());
  }

  if (ShouldShowSelectorDot()) {
    CreateSelectorDot();
  }
}

PaginationView::~PaginationView() = default;

gfx::Size PaginationView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int total_pages = model_->total_pages();
  if (total_pages < kMinNumPages) {
    return gfx::Size();
  }

  // Initialize container size with indicator container size.
  int container_size = GetIndicatorContainerWidth(total_pages);
  if (total_pages > kMaxNumVisibleIndicators) {
    // If the number of total pages exceeds visible maximum, add arrow buttons.
    container_size += 2 * (kArrowButtonIconSize + kArrowIndicatorSpacing);
  }

  return (orientation_ == Orientation::kHorizontal)
             ? gfx::Size(container_size, kIndicatorButtonSize)
             : gfx::Size(kIndicatorButtonSize, container_size);
}

void PaginationView::Layout(PassKey) {
  const bool horizontal = (orientation_ == Orientation::kHorizontal);
  int offset = 0;

  // A callback to set the bounds of given arrow button if it exists. Return the
  // button size if the button exists. Otherwise, return 0.
  auto set_arrow_button = [&](views::ImageButton* arrow_button) -> int {
    if (arrow_button) {
      gfx::Point origin =
          horizontal ? gfx::Point(offset, 0) : gfx::Point(0, offset);
      arrow_button->SetBoundsRect(gfx::Rect(
          origin, gfx::Size(kArrowButtonIconSize, kArrowButtonIconSize)));
      return kArrowButtonIconSize;
    }
    return 0;
  };

  // Set the backward arrow button if exists.
  offset += set_arrow_button(backward_arrow_button_);

  // Set the indicator container.
  indicator_container_->SizeToPreferredSize();
  const int scroll_view_size =
      GetIndicatorContainerWidth(model_->total_pages());
  if (horizontal) {
    indicator_scroll_view_->SetBounds(offset, 0, scroll_view_size,
                                      kIndicatorButtonSize);
  } else {
    indicator_scroll_view_->SetBounds(0, offset, kIndicatorButtonSize,
                                      scroll_view_size);
  }
  offset += scroll_view_size + kArrowIndicatorSpacing;

  // Set the right arrow button if exists.
  set_arrow_button(forward_arrow_button_);

  // Update arrow button visibility and selector dot position.
  UpdateArrowButtonsVisiblity();
  UpdateSelectorDot();
}

void PaginationView::CreateArrowButtons() {
  const bool horizontal = (orientation_ == Orientation::kHorizontal);
  for (bool forward : {true, false}) {
    auto arrow_button = std::make_unique<views::ImageButton>(
        base::BindRepeating(&PaginationView::OnArrowButtonPressed,
                            base::Unretained(this), forward));

    arrow_button->SetImageModel(
        views::ImageButton::ButtonState::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            forward
                ? (horizontal ? kOverflowShelfRightIcon : kChevronDownSmallIcon)
                : (horizontal ? kOverflowShelfLeftIcon : kChevronUpSmallIcon),
            kArrowButtonColorId, kArrowButtonIconSize));

    if (forward) {
      arrow_button->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_PAGINATION_FORWARD_ARROW_TOOLTIP));
      forward_arrow_button_ = AddChildView(std::move(arrow_button));
    } else {
      arrow_button->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_PAGINATION_BACKWARD_ARROW_TOOLTIP));
      backward_arrow_button_ = AddChildView(std::move(arrow_button));
    }
  }
}

void PaginationView::RemoveArrowButtons() {
  RemoveChildViewT(std::exchange(backward_arrow_button_, nullptr));
  RemoveChildViewT(std::exchange(forward_arrow_button_, nullptr));
}

void PaginationView::UpdateArrowButtonsVisiblity() {
  // If the first page indicator is visible, hide the left arrow button.
  if (backward_arrow_button_) {
    backward_arrow_button_->SetVisible(!IsIndicatorVisible(0));
  }

  // If the last page indicator is visible, hide the right arrow button.
  if (forward_arrow_button_) {
    forward_arrow_button_->SetVisible(
        !IsIndicatorVisible(model_->total_pages() - 1));
  }
}

void PaginationView::OnArrowButtonPressed(bool forward,
                                          const ui::Event& event) {
  const int page_offset = forward ? 1 : -1;
  model_->SelectPage(model_->selected_page() + page_offset, /*animate=*/true);
}

void PaginationView::MaybeSetUpScroll() {
  const int current_page = model_->selected_page();
  const int target_page = model_->transition().target_page;
  if (!model_->is_valid_page(current_page) ||
      !model_->is_valid_page(target_page)) {
    return;
  }

  // If the target page indicator is not in visible area, scroll the container.
  if (!IsIndicatorVisible(target_page)) {
    indicator_container_->StartScroll(current_page, target_page);
  }
}

bool PaginationView::ShouldShowSelectorDot() const {
  return model_->total_pages() >= kMinNumPages &&
         model_->is_valid_page(model_->selected_page());
}

void PaginationView::CreateSelectorDot() {
  if (selector_dot_) {
    return;
  }

  selector_dot_ =
      indicator_container_->AddChildView(std::make_unique<SelectorDotView>());
  UpdateSelectorDot();
}

void PaginationView::RemoveSelectorDot() {
  if (!selector_dot_) {
    return;
  }

  indicator_container_->RemoveChildViewT(std::exchange(selector_dot_, nullptr));
}

void PaginationView::UpdateSelectorDot() {
  if (!selector_dot_) {
    return;
  }

  // The selected page may become invalid when total pages is changing.
  const int selected_page = model_->selected_page();
  if (!model_->is_valid_page(selected_page)) {
    return;
  }

  // Move the selector dot to the position of selected page indicator if the
  // selector dot is not deforming.
  if (!selector_dot_->DeformingInProgress()) {
    selector_dot_->SetBoundsRect(
        indicator_container_->GetIndicatorByPage(selected_page)
            ->GetIndicatorBounds());
  }
}

void PaginationView::SetUpSelectorDotDeformation() {
  CHECK(selector_dot_);
  CHECK(!selector_dot_->DeformingInProgress());

  const int current_page = model_->selected_page();
  const int target_page = model_->transition().target_page;

  if (!model_->is_valid_page(current_page) ||
      !model_->is_valid_page(target_page)) {
    return;
  }

  const gfx::Rect current_bounds =
      indicator_container_->GetIndicatorByPage(current_page)
          ->GetIndicatorBounds();
  const gfx::Rect target_bounds =
      indicator_container_->GetIndicatorByPage(target_page)
          ->GetIndicatorBounds();
  // If moves to a neighbor page, the selector dot will first be stretched into
  // a pill shape until it connects the current indicator to the target
  // indicator, and then shrink back to a circle at the target indicator
  // position.
  if (std::abs(target_page - current_page) == 1) {
    const gfx::Rect intermediate_bounds =
        gfx::UnionRects(current_bounds, target_bounds);
    selector_dot_->AddDeformInterval(
        {0.0, current_bounds, 0.5, intermediate_bounds});
    selector_dot_->AddDeformInterval(
        {0.5, intermediate_bounds, 1.0, target_bounds});
    return;
  }

  // If jumps across multiple pages, the selector dot will first shrink at the
  // current indicator position, and then expand at the target indicator
  // position.
  selector_dot_->AddDeformInterval(
      {0.0, current_bounds, 0.5,
       gfx::Rect(current_bounds.CenterPoint(), gfx::Size())});
  selector_dot_->AddDeformInterval(
      {0.5, gfx::Rect(target_bounds.CenterPoint(), gfx::Size()), 1.0,
       target_bounds});
}

bool PaginationView::IsIndicatorVisible(int page) const {
  // Check if the indicator is in the visible rect of the scroll view.
  return indicator_scroll_view_->GetVisibleRect().Contains(
      indicator_container_->GetIndicatorByPage(page)->bounds());
}

void PaginationView::SelectedPageChanged(int old_selected, int new_selected) {
  // Update selector dot position and arrow buttons visibility.
  if (ShouldShowSelectorDot()) {
    if (!selector_dot_) {
      CreateSelectorDot();
    } else {
      // Finish and reset ongoing deformation.
      selector_dot_->ResetDeform(/*canceled=*/false);
    }
  } else {
    RemoveSelectorDot();
  }

  // Finish and reset ongoing indicator container scrolling.
  if (indicator_container_->ScrollingInProgress()) {
    indicator_container_->ResetScroll(/*canceled=*/false);
    UpdateArrowButtonsVisiblity();
  }
}

void PaginationView::TotalPagesChanged(int previous_page_count,
                                       int new_page_count) {
  const int current_indicator_num =
      indicator_container_->GetNumberOfIndicators();
  new_page_count = new_page_count < kMinNumPages ? 0 : new_page_count;
  if (current_indicator_num == new_page_count) {
    return;
  }

  if (current_indicator_num < new_page_count) {
    // Add more indicators at the end of container.
    for (int i = current_indicator_num; i < new_page_count; i++) {
      indicator_container_->PushIndicator(model_.get());
    }

    // Add arrow buttons if the number of total pages exceeds the visible
    // maximum.
    if (current_indicator_num <= kMaxNumVisibleIndicators &&
        new_page_count > kMaxNumVisibleIndicators) {
      CreateArrowButtons();
    }

    // Create selector dot if the number of total pages exceeds the minimum
    // number of pages.
    if (new_page_count >= kMinNumPages && !selector_dot_) {
      CreateSelectorDot();
    }
  } else {
    // Remove indicators from the end of the container.
    for (int i = current_indicator_num; i > new_page_count; i--) {
      indicator_container_->PopIndicator();
    }

    // Remove arrow buttons if the number of total pages does not exceed the
    // visible maximum.
    if (current_indicator_num > kMaxNumVisibleIndicators &&
        new_page_count <= kMaxNumVisibleIndicators) {
      RemoveArrowButtons();
    }

    // Remove the selector dot if the number of pages is less than the minimum
    // number of pages.
    if (new_page_count < kMinNumPages && selector_dot_) {
      RemoveSelectorDot();
    }
  }

  DeprecatedLayoutImmediately();
}

void PaginationView::TransitionChanged() {
  if (!selector_dot_) {
    return;
  }

  // If there is no transition, reset and cancel current selector dot
  // deformation and indicator container scrolling.
  if (!model_->has_transition()) {
    selector_dot_->ResetDeform(/*canceled=*/true);
    indicator_container_->ResetScroll(/*canceled=*/true);
    return;
  }

  const double progress = model_->transition().progress;

  // Scroll the indicator container if needed.
  if (!indicator_container_->ScrollingInProgress()) {
    MaybeSetUpScroll();
  }
  indicator_container_->Scroll(progress);

  // Deform the selector dot.
  if (!selector_dot_->DeformingInProgress()) {
    SetUpSelectorDotDeformation();
  }
  // Deform the selector dot.
  selector_dot_->Deform(progress);
}

BEGIN_METADATA(PaginationView)
END_METADATA
}  // namespace ash
