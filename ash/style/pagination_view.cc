// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/pagination_view.h"

#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "base/i18n/number_formatting.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

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
 public:
  METADATA_HEADER(IndicatorButton);

  IndicatorButton(PressedCallback callback,
                  const std::u16string& accessible_name)
      : views::Button(callback) {
    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
    SetAccessibleName(accessible_name);
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
  gfx::Size CalculatePreferredSize() const override {
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

BEGIN_METADATA(IndicatorButton, views::Button)
END_METADATA

}  // namespace

//------------------------------------------------------------------------------
// PaginationView::SelectorDotView:
// A solid circle that performs deformation with the pace of page transition.
class PaginationView::SelectorDotView : public views::View {
 public:
  METADATA_HEADER(SelectorDotView);

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

BEGIN_METADATA(PaginationView, SelectorDotView, views::View)
END_METADATA

//------------------------------------------------------------------------------
// PaginationView::IndicatorContainer:
// The container of indicators. If the indicator to be selected is not visible,
// the container will scroll with the pace of pagination transition.
class PaginationView::IndicatorContainer : public views::BoxLayoutView {
 public:
  METADATA_HEADER(IndicatorContainer);

  IndicatorContainer() {
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
    RemoveChildViewT(indicator_button);
  }

  // Gets indicator corresponding to the given page.
  IndicatorButton* GetIndicatorByPage(int page) {
    DCHECK_GE(page, 0);
    DCHECK_LT(page, static_cast<int>(buttons_.size()));
    return buttons_[page].get();
  }

  // Sets up scrolling if an invisible page is selected.
  void StartScroll(int start_page, int target_page) {
    // Scroll the indicator container by the distance of a indicator button size
    // plus button spacing to reveal the next/previous indicator.
    // TODO(zxdan): settings bounds at each step will cause repainting which is
    // expensive. However, using transform sometimes makes the stroke of
    // indicator circle become thicker. Will investigate the cause latter.
    const bool left = start_page < target_page;
    const int start_page_offset =
        left ? kMaxNumVisibleIndicators - start_page - 1 : -start_page;
    const int target_page_offset =
        left ? kMaxNumVisibleIndicators - target_page - 1 : -target_page;
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
    SetX(gfx::Tween::IntValueBetween(progress, scroll_interval_->start_value,
                                     scroll_interval_->target_value));
  }

  void ResetScroll(bool canceled) {
    if (scroll_interval_) {
      SetX(canceled ? scroll_interval_->start_value
                    : scroll_interval_->target_value);
    }
    scroll_interval_ = absl::nullopt;
  }

  // Returns true if the scrolling is in progress.
  bool ScrollingInProgress() { return !!scroll_interval_; }

 private:
  std::vector<base::raw_ptr<IndicatorButton>> buttons_;
  absl::optional<InterpolationInterval<int>> scroll_interval_;
};

BEGIN_METADATA(PaginationView, IndicatorContainer, views::BoxLayoutView)
END_METADATA

//------------------------------------------------------------------------------
// PaginationView:
PaginationView::PaginationView(PaginationModel* model)
    : model_(model),
      indicator_scroll_view_(
          AddChildView(std::make_unique<views::ScrollView>())),
      indicator_container_(indicator_scroll_view_->SetContents(
          std::make_unique<IndicatorContainer>())) {
  DCHECK(model_);

  model_observation_.Observe(model_.get());

  // The scroll view does not accept scroll event.
  indicator_scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  indicator_scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);

  TotalPagesChanged(0, model_->total_pages());

  if (model_->is_valid_page(model_->selected_page())) {
    CreateSelectorDot();
  }
}

PaginationView::~PaginationView() = default;

gfx::Size PaginationView::CalculatePreferredSize() const {
  const int total_pages = model_->total_pages();
  const int visible_num = std::min(total_pages, kMaxNumVisibleIndicators);
  const int container_width = visible_num * kIndicatorButtonSize +
                              (visible_num - 1) * kIndicatorSpacing;

  // If the number of total pages does not exceed visible maximum, only show
  // indicator container.
  if (total_pages <= visible_num) {
    return gfx::Size(container_width, kIndicatorButtonSize);
  }

  // Otherwise, show indicator container and arrow buttons.
  return gfx::Size(
      container_width + 2 * (kArrowButtonIconSize + kArrowIndicatorSpacing),
      kIndicatorButtonSize);
}

void PaginationView::Layout() {
  int offset_x = 0;
  // Set the left arrow button if exists.
  if (left_arrow_button_) {
    left_arrow_button_->SetBounds(offset_x, 0, kArrowButtonIconSize,
                                  kArrowButtonIconSize);
    offset_x += left_arrow_button_->width() + kArrowIndicatorSpacing;
  }

  // Set the indicator container.
  indicator_container_->SizeToPreferredSize();
  const int visible_num =
      std::min(model_->total_pages(), kMaxNumVisibleIndicators);
  indicator_scroll_view_->SetBounds(offset_x, 0,
                                    visible_num * kIndicatorButtonSize +
                                        (visible_num - 1) * kIndicatorSpacing,
                                    kIndicatorButtonSize);

  offset_x += indicator_scroll_view_->width() + kArrowIndicatorSpacing;

  // Set the right arrow button if exists.
  if (right_arrow_button_) {
    right_arrow_button_->SetBounds(offset_x, 0, kIndicatorButtonSize,
                                   kIndicatorButtonSize);
  }

  // Update arrow button visibility and selector dot position.
  UpdateArrowButtonsVisiblity();
  UpdateSelectorDot();
}

void PaginationView::CreateArrowButtons() {
  for (bool left : {true, false}) {
    auto arrow_button = std::make_unique<views::ImageButton>(
        base::BindRepeating(&PaginationView::OnArrowButtonPressed,
                            base::Unretained(this), left));

    arrow_button->SetImageModel(
        views::ImageButton::ButtonState::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            left ? kOverflowShelfLeftIcon : kOverflowShelfRightIcon,
            kArrowButtonColorId, kArrowButtonIconSize));

    if (left) {
      arrow_button->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_PAGINATION_LEFT_ARROW_TOOLTIP));
      left_arrow_button_ = AddChildView(std::move(arrow_button));
    } else {
      arrow_button->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_PAGINATION_RIGHT_ARROW_TOOLTIP));
      right_arrow_button_ = AddChildView(std::move(arrow_button));
    }
  }
}

void PaginationView::RemoveArrowButtons() {
  RemoveChildViewT(left_arrow_button_);
  left_arrow_button_ = nullptr;

  RemoveChildViewT(right_arrow_button_);
  right_arrow_button_ = nullptr;
}

void PaginationView::UpdateArrowButtonsVisiblity() {
  // If the first page indicator is visible, hide the left arrow button.
  if (left_arrow_button_) {
    left_arrow_button_->SetVisible(!IsIndicatorVisible(0));
  }

  // If the last page indicator is visible, hide the right arrow button.
  if (right_arrow_button_) {
    right_arrow_button_->SetVisible(
        !IsIndicatorVisible(model_->total_pages() - 1));
  }
}

void PaginationView::OnArrowButtonPressed(bool left, const ui::Event& event) {
  const int page_offset = left ? -1 : 1;
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

  indicator_container_->RemoveChildViewT(selector_dot_);
  selector_dot_ = nullptr;
}

void PaginationView::UpdateSelectorDot() {
  if (!selector_dot_) {
    return;
  }

  // Move the selector dot to the position of selected page indicator if the
  // selector dot is not deforming.
  const int selected_page = model_->selected_page();
  DCHECK(model_->is_valid_page(selected_page));
  if (!selector_dot_->DeformingInProgress()) {
    selector_dot_->SetBoundsRect(
        indicator_container_->GetIndicatorByPage(selected_page)
            ->GetIndicatorBounds());
  }
}

void PaginationView::SetUpSelectorDotDeformation() {
  DCHECK(!selector_dot_->DeformingInProgress());

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
  if (model_->is_valid_page(new_selected)) {
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
  if (previous_page_count < new_page_count) {
    // Add more indicators at the end of container.
    for (int i = previous_page_count; i < new_page_count; i++) {
      indicator_container_->PushIndicator(model_.get());
    }

    // Add arrow buttons if the number of total pages exceeds the visible
    // maximum.
    if (previous_page_count <= kMaxNumVisibleIndicators &&
        new_page_count > kMaxNumVisibleIndicators) {
      CreateArrowButtons();
    }
  } else {
    // Remove indicators from the end of the container.
    for (int i = previous_page_count; i > new_page_count; i--) {
      indicator_container_->PopIndicator();
    }

    // Remove arrow buttons if the number of total pages does not exceed the
    // visible maximum.
    if (previous_page_count > kMaxNumVisibleIndicators &&
        new_page_count <= kMaxNumVisibleIndicators) {
      RemoveArrowButtons();
    }

    // Remove the selector dot if there is no pages.
    if (new_page_count == 0) {
      RemoveSelectorDot();
    }
  }

  Layout();
}

void PaginationView::TransitionChanged() {
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

BEGIN_METADATA(PaginationView, views::View)
END_METADATA
}  // namespace ash
