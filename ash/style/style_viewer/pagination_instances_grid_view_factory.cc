// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "ash/style/pagination_view.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

// Configurations of grid view for `Pagination` instances.
constexpr size_t kGridViewRowNum = 3;
constexpr size_t kGridViewColNum = 2;
constexpr size_t kGridViewRowGroupSize = 1;
constexpr size_t kGridViewColGroupSize = 1;

// The size of a test page.
constexpr size_t kPageWidth = 100;
constexpr size_t kPageHeight = 30;

//------------------------------------------------------------------------------
// PaginationTestContents:
// A view binded with a pagination controller. It dispatches the dragging event
// to the pagination controller.
class PaginationTestContents : public views::BoxLayoutView {
 public:
  PaginationTestContents(PaginationController* pagination_controller,
                         views::BoxLayout::Orientation orientation)
      : pagination_controller_(pagination_controller) {
    SetOrientation(orientation);
  }
  PaginationTestContents(const PaginationTestContents&) = delete;
  PaginationTestContents& operator=(const PaginationTestContents&) = delete;
  ~PaginationTestContents() override = default;

  // views::BoxLayoutView:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Record potential dragging origin.
    dragging_origin_ = event.target()->GetScreenLocationF(event);
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    gfx::PointF dragging_pos = event.target()->GetScreenLocationF(event);
    gfx::Vector2dF offset = dragging_pos - dragging_origin_;
    if (!is_dragging_) {
      pagination_controller_->StartMouseDrag(offset);
      is_dragging_ = true;
    } else {
      pagination_controller_->UpdateMouseDrag(
          offset, gfx::Rect(0, 0, kPageWidth, kPageHeight));
    }
    dragging_origin_ = dragging_pos;
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (!is_dragging_) {
      return;
    }

    pagination_controller_->EndMouseDrag(event);
    is_dragging_ = false;
  }

 private:
  // The current dragging location.
  gfx::PointF dragging_origin_;
  // True if the content is being dragged.
  bool is_dragging_ = false;
  raw_ptr<PaginationController> const pagination_controller_;
};

//------------------------------------------------------------------------------
// PaginationTestScrollView:
// A scroll view with as many labels as the number of pages in a pagination
// model. Each label corresponds to a page. Eveytime a page is selected, the
// view will scroll to show corresponding label.
class PaginationTestScrollView : public views::ScrollView,
                                 public PaginationModelObserver {
 public:
  PaginationTestScrollView(PaginationModel* model,
                           PaginationView::Orientation orientation)
      : model_(model),
        orientation_(orientation),
        pagination_controller_(std::make_unique<PaginationController>(
            model_,
            (orientation == PaginationView::Orientation::kHorizontal)
                ? PaginationController::SCROLL_AXIS_HORIZONTAL
                : PaginationController::SCROLL_AXIS_VERTICAL,
            base::BindRepeating([](ui::EventType) {}))),
        page_container_(SetContents(std::make_unique<PaginationTestContents>(
            pagination_controller_.get(),
            (orientation == PaginationView::Orientation::kHorizontal)
                ? views::BoxLayout::Orientation::kHorizontal
                : views::BoxLayout::Orientation::kVertical))) {
    model_observer_.Observe(model_);
    SetHorizontalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled);
    SetVerticalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled);
    if (model_->total_pages() > 0) {
      TotalPagesChanged(0, model_->total_pages());
    }
  }

  PaginationTestScrollView(const PaginationTestScrollView&) = delete;
  PaginationTestScrollView& operator=(const PaginationTestScrollView&) = delete;
  ~PaginationTestScrollView() override = default;

  // views::ScrollView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kPageWidth, kPageHeight);
  }

  void Layout(PassKey) override {
    page_container_->SizeToPreferredSize();
    LayoutSuperclass<views::ScrollView>(this);
  }

  // PaginationModelObserver:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override {
    previous_page_count = std::max(0, previous_page_count);
    new_page_count = std::max(0, new_page_count);
    if (previous_page_count == new_page_count) {
      return;
    }

    // Synchronize the number of labels with total pages.
    if (previous_page_count < new_page_count) {
      for (int i = previous_page_count; i < new_page_count; i++) {
        auto* page =
            page_container_->AddChildView(std::make_unique<views::Label>(
                u"Page " + base::NumberToString16(i + 1)));
        page->SetPreferredSize(gfx::Size(kPageWidth, kPageHeight));
        page->SetLineHeight(kPageHeight);
      }
    } else {
      for (int i = previous_page_count; i > new_page_count; i--) {
        page_container_->RemoveChildView(page_container_->children().back());
      }
    }
  }

  void SelectedPageChanged(int old_selected, int new_selected) override {
    // Scroll to show the label corresponding to the selected page.
    if (model_->is_valid_page(new_selected)) {
      if (orientation_ == PaginationView::Orientation::kHorizontal) {
        page_container_->SetX(-new_selected * kPageWidth);
      } else {
        page_container_->SetY(-new_selected * kPageHeight);
      }
    }
  }

  void TransitionChanged() override {
    // Update scrolling during the page transition.
    const bool horizontal =
        (orientation_ == PaginationView::Orientation::kHorizontal);
    const int page_size = horizontal ? kPageWidth : kPageHeight;
    const int origin_offset = -model_->selected_page() * page_size;
    const int target_offset = -model_->transition().target_page * page_size;
    const double progress = model_->transition().progress;
    const int offset =
        (1 - progress) * origin_offset + progress * target_offset;
    if (orientation_ == PaginationView::Orientation::kHorizontal) {
      page_container_->SetX(offset);
    } else {
      page_container_->SetY(offset);
    }
  }

 private:
  raw_ptr<PaginationModel> const model_;
  PaginationView::Orientation orientation_;
  std::unique_ptr<PaginationController> pagination_controller_;
  raw_ptr<PaginationTestContents> page_container_;
  base::ScopedObservation<PaginationModel, PaginationModelObserver>
      model_observer_{this};
};

//------------------------------------------------------------------------------
// PaginationGridView:
class PaginationGridView : public SystemUIComponentsGridView {
 public:
  PaginationGridView(size_t num_row, size_t num_col)
      : SystemUIComponentsGridView(num_row, num_col, num_row, num_col) {}
  PaginationGridView(const PaginationGridView&) = delete;
  PaginationGridView& operator=(const PaginationGridView&) = delete;
  ~PaginationGridView() override = default;

  // Add a pagination instance and a test view binded with the given pagination
  // model.
  void AddPaginationWithModel(
      const std::u16string& name,
      PaginationView::Orientation orientation,
      std::unique_ptr<PaginationModel> pagination_model) {
    AddInstance(name, std::make_unique<PaginationView>(pagination_model.get(),
                                                       orientation));
    AddInstance(u"", std::make_unique<PaginationTestScrollView>(
                         pagination_model.get(), orientation));
    model_ = std::move(pagination_model);
  }

 private:
  std::unique_ptr<PaginationModel> model_;
};

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreatePaginationInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGridViewColGroupSize);

  // Add a horizontal pagination view with 3 pages.
  auto* horizontal_instance_grid_view_1 =
      grid_view->AddInstance(u"", std::make_unique<PaginationGridView>(2, 1));
  auto model_three_horizontal = std::make_unique<PaginationModel>(nullptr);
  model_three_horizontal->SetTotalPages(3);
  horizontal_instance_grid_view_1->AddPaginationWithModel(
      u"Horizontal pagenation with 3 pages",
      PaginationView::Orientation::kHorizontal,
      std::move(model_three_horizontal));

  // Add a vertical pagination view with 3 pages.
  auto* vertical_instance_grid_view_1 =
      grid_view->AddInstance(u"", std::make_unique<PaginationGridView>(1, 2));
  auto model_three_vertical = std::make_unique<PaginationModel>(nullptr);
  model_three_vertical->SetTotalPages(3);
  vertical_instance_grid_view_1->AddPaginationWithModel(
      u"Vertical pagenation with 3 pages",
      PaginationView::Orientation::kVertical, std::move(model_three_vertical));

  // Add a Horizontal pagination view with 5 pages.
  auto* horizontal_instance_grid_view_2 =
      grid_view->AddInstance(u"", std::make_unique<PaginationGridView>(2, 1));
  auto model_five_horizontal = std::make_unique<PaginationModel>(nullptr);
  model_five_horizontal->SetTotalPages(5);
  horizontal_instance_grid_view_2->AddPaginationWithModel(
      u"Pagenation with 5 pages", PaginationView::Orientation::kHorizontal,
      std::move(model_five_horizontal));

  // Add a vertical pagination view with 5 pages.
  auto* vertical_instance_grid_view_2 =
      grid_view->AddInstance(u"", std::make_unique<PaginationGridView>(1, 2));
  auto model_five_vertical = std::make_unique<PaginationModel>(nullptr);
  model_five_vertical->SetTotalPages(5);
  vertical_instance_grid_view_2->AddPaginationWithModel(
      u"Vertical pagenation with 5 pages",
      PaginationView::Orientation::kVertical, std::move(model_five_vertical));

  // Add a horizontal pagination view with 10 pages.
  auto* horizontal_instance_grid_view_3 =
      grid_view->AddInstance(u"", std::make_unique<PaginationGridView>(2, 1));
  auto model_ten_horizontal = std::make_unique<PaginationModel>(nullptr);
  model_ten_horizontal->SetTotalPages(10);
  horizontal_instance_grid_view_3->AddPaginationWithModel(
      u"Pagenation with 10 pages", PaginationView::Orientation::kHorizontal,
      std::move(model_ten_horizontal));

  // Add a vertical pagination view with 10 pages.
  auto* vertical_instance_grid_view_3 =
      grid_view->AddInstance(u"", std::make_unique<PaginationGridView>(1, 2));
  auto model_ten_vertical = std::make_unique<PaginationModel>(nullptr);
  model_ten_vertical->SetTotalPages(10);
  vertical_instance_grid_view_3->AddPaginationWithModel(
      u"Vertical pagenation with 10 pages",
      PaginationView::Orientation::kVertical, std::move(model_ten_vertical));

  return grid_view;
}

}  // namespace ash
