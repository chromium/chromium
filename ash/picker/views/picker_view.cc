// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include <memory>

#include "ash/bubble/bubble_event_filter.h"
#include "ash/picker/model/picker_category.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_category_view.h"
#include "ash/picker/views/picker_contents_view.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ash/picker/views/picker_user_education_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/picker/views/picker_zero_state_view.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace ash {
namespace {

constexpr gfx::Size kPickerSize(320, 340);
constexpr int kBorderRadius = 12;
constexpr int kShadowElevation = 3;
constexpr ui::ColorId kBackgroundColor =
    cros_tokens::kCrosSysSystemBaseElevated;

std::unique_ptr<views::BubbleBorder> CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
  border->SetCornerRadius(kBorderRadius);
  border->set_md_shadow_elevation(kShadowElevation);
  return border;
}

std::unique_ptr<views::Separator> CreateSeparator() {
  return views::Builder<views::Separator>()
      .SetOrientation(views::Separator::Orientation::kHorizontal)
      .SetColorId(cros_tokens::kCrosSysSeparator)
      .Build();
}

// Gets the preferred layout to use given `caret_bounds` in screen coordinates.
PickerView::PickerLayoutType GetLayoutType(const gfx::Rect& caret_bounds) {
  return caret_bounds.bottom() + kPickerSize.height() <=
                 display::Screen::GetScreen()
                     ->GetDisplayMatching(caret_bounds)
                     .work_area()
                     .bottom()
             ? PickerView::PickerLayoutType::kResultsBelowSearchField
             : PickerView::PickerLayoutType::kResultsAboveSearchField;
}

// Gets the preferred Picker widget bounds given the preferred client bounds
// (i.e. Picker view bounds), in screen coordinates.
gfx::Rect GetPickerWidgetBoundsForClientBounds(const gfx::Rect& client_bounds) {
  gfx::Rect non_client_frame_view_bounds = client_bounds;
  non_client_frame_view_bounds.Inset(-CreateBorder()->GetInsets());
  return non_client_frame_view_bounds;
}

// Gets the preferred Picker view bounds in screen coordinates. We try to place
// the Picker view close to `caret_bounds`, while taking into account
// `layout_type`, `picker_view_size` and available space on the screen.
// `picker_view_search_field_vertical_offset` is the vertical offset from the
// top of the Picker view to the center of the search field, which we use to try
// to align the search field with the center of the caret bounds.
gfx::Rect GetPickerViewBounds(const gfx::Rect& caret_bounds,
                              PickerView::PickerLayoutType layout_type,
                              const gfx::Size& picker_view_size,
                              int picker_view_search_field_vertical_offset) {
  // Place the Picker in available screen space near the caret bounds.
  const gfx::Rect screen_work_area = display::Screen::GetScreen()
                                         ->GetDisplayMatching(caret_bounds)
                                         .work_area();
  gfx::Rect picker_view_bounds(picker_view_size);
  if (caret_bounds.right() + picker_view_size.width() <=
      screen_work_area.right()) {
    // If there is space, place the Picker to the right of the caret, vertically
    // aligning the center of the Picker search field with the center of the
    // caret.
    picker_view_bounds.set_origin(caret_bounds.right_center());
    picker_view_bounds.Offset(0, -picker_view_search_field_vertical_offset);
  } else {
    switch (layout_type) {
      case PickerView::PickerLayoutType::kResultsBelowSearchField:
        // Try to place the Picker at the right edge of the screen, below
        // the caret.
        picker_view_bounds.set_origin(
            {screen_work_area.right() - picker_view_size.width(),
             caret_bounds.bottom()});
        break;
      case PickerView::PickerLayoutType::kResultsAboveSearchField:
        // Try to place the Picker at the right edge of the screen, above
        // the caret.
        picker_view_bounds.set_origin(
            {screen_work_area.right() - picker_view_size.width(),
             caret_bounds.y() - picker_view_size.height()});
        break;
    }
  }

  // Adjust if necessary to keep the whole Picker view onscreen. Note that the
  // non client area of the Picker, e.g. the shadows, are allowed to be
  // offscreen.
  picker_view_bounds.AdjustToFit(screen_work_area);
  return picker_view_bounds;
}

}  // namespace

PickerView::PickerView(PickerViewDelegate* delegate,
                       const base::TimeTicks trigger_event_timestamp,
                       PickerLayoutType layout_type)
    : session_metrics_(trigger_event_timestamp), delegate_(delegate) {
  SetShowCloseButton(false);
  SetBackground(views::CreateThemedSolidBackground(kBackgroundColor));
  SetPreferredSize(kPickerSize);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  switch (layout_type) {
    case PickerLayoutType::kResultsBelowSearchField:
      AddSearchFieldView();
      AddChildView(CreateSeparator());
      AddContentsView();
      AddChildView(std::make_unique<PickerUserEducationView>());
      break;
    case PickerLayoutType::kResultsAboveSearchField:
      AddChildView(std::make_unique<PickerUserEducationView>());
      AddContentsView();
      AddChildView(CreateSeparator());
      AddSearchFieldView();
      break;
  }

  // Automatically focus on the search field.
  SetInitiallyFocusedView(search_field_view_);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

PickerView::~PickerView() = default;

views::UniqueWidgetPtr PickerView::CreateWidget(
    const gfx::Rect& caret_bounds,
    PickerViewDelegate* delegate,
    const base::TimeTicks trigger_event_timestamp) {
  // Create the Picker view and set its size. This will trigger a layout, so
  // that the position of the Picker view's search field can be used when
  // setting the Picker widget bounds below.
  const auto layout_type = GetLayoutType(caret_bounds);
  auto picker_view = std::make_unique<PickerView>(
      delegate, trigger_event_timestamp, layout_type);
  picker_view->SetSize(kPickerSize);

  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.type = views::Widget::InitParams::TYPE_BUBBLE;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.bounds = GetPickerWidgetBoundsForClientBounds(
      picker_view->GetTargetBounds(caret_bounds, layout_type));
  // TODO(b/309706053): Replace this with the finalized string.
  params.name = "Picker";
  params.delegate = picker_view.release();

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_HIDE);
  return widget;
}

bool PickerView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  CHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  if (auto* widget = GetWidget()) {
    widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  }
  return true;
}

void PickerView::PaintChildren(const views::PaintInfo& paint_info) {
  if (delegate_->ShouldPaint()) {
    views::View::PaintChildren(paint_info);
  }
}

std::unique_ptr<views::NonClientFrameView> PickerView::CreateNonClientFrameView(
    views::Widget* widget) {
  auto frame =
      std::make_unique<views::BubbleFrameView>(gfx::Insets(), gfx::Insets());
  frame->SetBubbleBorder(CreateBorder());
  return frame;
}

void PickerView::AddedToWidget() {
  session_metrics_.StartRecording(*GetWidget());
  // `base::Unretained` is safe here because this class owns
  // `bubble_event_filter_`.
  bubble_event_filter_ = std::make_unique<BubbleEventFilter>(
      GetWidget(), /*button=*/nullptr,
      base::BindRepeating(&PickerView::OnClickOutsideWidget,
                          base::Unretained(this)));
}

void PickerView::RemovedFromWidget() {
  session_metrics_.StopRecording();
  bubble_event_filter_.reset();
}

gfx::Rect PickerView::GetTargetBounds(const gfx::Rect& caret_bounds,
                                      PickerLayoutType layout_type) {
  return GetPickerViewBounds(caret_bounds, layout_type, size(),
                             search_field_view_->bounds().CenterPoint().y());
}

void PickerView::StartSearch(const std::u16string& query) {
  if (!query.empty()) {
    contents_view_->SetActivePage(search_results_view_);
    delegate_->StartSearch(
        query, selected_category_,
        base::BindRepeating(&PickerView::PublishSearchResults,
                            weak_ptr_factory_.GetWeakPtr()));
  } else if (selected_category_.has_value()) {
    contents_view_->SetActivePage(category_view_);
  } else {
    contents_view_->SetActivePage(zero_state_view_);
  }
}

void PickerView::PublishSearchResults(const PickerSearchResults& results) {
  search_results_view_->SetSearchResults(results);
  session_metrics_.MarkSearchResultsUpdated();
}

void PickerView::SelectSearchResult(const PickerSearchResult& result) {
  delegate_->InsertResultOnNextFocus(result);
  GetWidget()->Close();
}

void PickerView::SelectCategory(PickerCategory category) {
  selected_category_ = category;
  contents_view_->SetActivePage(category_view_);
  delegate_->GetResultsForCategory(
      category, base::BindRepeating(&PickerView::PublishCategoryResults,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void PickerView::PublishCategoryResults(const PickerSearchResults& results) {
  category_view_->SetResults(results);
}

void PickerView::OnClickOutsideWidget() {
  if (auto* widget = GetWidget()) {
    widget->Close();
  }
}

void PickerView::AddSearchFieldView() {
  // `base::Unretained` is safe here because this class owns
  // `search_field_view_`.
  search_field_view_ = AddChildView(std::make_unique<PickerSearchFieldView>(
      base::BindRepeating(&PickerView::StartSearch, base::Unretained(this)),
      &session_metrics_));
}

void PickerView::AddContentsView() {
  contents_view_ = AddChildView(std::make_unique<PickerContentsView>());
  contents_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));

  // `base::Unretained` is safe here because this class owns
  // `zero_state_view_`, `category_view_` and `search_results_view`_.
  zero_state_view_ = contents_view_->AddPage(
      std::make_unique<PickerZeroStateView>(base::BindRepeating(
          &PickerView::SelectCategory, base::Unretained(this))));
  category_view_ = contents_view_->AddPage(std::make_unique<PickerCategoryView>(
      base::BindOnce(&PickerView::SelectSearchResult, base::Unretained(this)),
      delegate_->GetAssetFetcher()));
  search_results_view_ =
      contents_view_->AddPage(std::make_unique<PickerSearchResultsView>(
          base::BindOnce(&PickerView::SelectSearchResult,
                         base::Unretained(this)),
          delegate_->GetAssetFetcher()));
  contents_view_->SetActivePage(zero_state_view_);
}

BEGIN_METADATA(PickerView, views::View)
END_METADATA

}  // namespace ash
