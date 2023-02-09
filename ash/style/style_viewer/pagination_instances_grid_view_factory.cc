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
constexpr size_t kGridViewRowNum = 6;
constexpr size_t kGridViewColNum = 1;
constexpr size_t kGridViewRowGroupSize = 2;
constexpr size_t kGirdViewColGroupSize = 1;

// The size of a test page.
constexpr size_t kPageWidth = 100;
constexpr size_t kPageHeight = 30;

//------------------------------------------------------------------------------
// PaginationTestContents:
// A view binded with a pagination controller. It dispatches the dragging event
// to the pagination controller.
class PaginationTestContents : public views::BoxLayoutView {
 public:
  PaginationTestContents(PaginationController* pagination_controller)
      : pagination_controller_(pagination_controller) {}
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
  base::raw_ptr<PaginationController> const pagination_controller_;
};

//------------------------------------------------------------------------------
// PaginationTestScrollView:
// A scroll view with as many labels as the number of pages in a pagination
// model. Each label corresponds to a page. Eveytime a page is selected, the
// view will scroll to show corresponding label.
class PaginationTestScrollView : public views::ScrollView,
                                 public PaginationModelObserver {
 public:
  PaginationTestScrollView(PaginationModel* model)
      : model_(model),
        pagination_controller_(std::make_unique<PaginationController>(
            model_,
            PaginationController::SCROLL_AXIS_HORIZONTAL,
            base::BindRepeating([](ui::EventType) {}))),
        page_container_(SetContents(std::make_unique<PaginationTestContents>(
            pagination_controller_.get()))) {
    model_observer_.Observe(model_);
    SetHorizontalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled);
    SetVerticalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled);
    TotalPagesChanged(0, model_->total_pages());
  }

  PaginationTestScrollView(const PaginationTestScrollView&) = delete;
  PaginationTestScrollView& operator=(const PaginationTestScrollView&) = delete;
  ~PaginationTestScrollView() override = default;

  // views::ScrollView:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kPageWidth, kPageHeight);
  }

  void Layout() override {
    page_container_->SizeToPreferredSize();
    views::ScrollView::Layout();
  }

  // PaginationModelObserver:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override {
    // Synchronize the number of labels with total pages.
    if (previous_page_count < new_page_count) {
      for (int i = previous_page_count; i < new_page_count; i++) {
        auto* page =
            page_container_->AddChildView(std::make_unique<views::Label>(
                u"Page " + base::NumberToString16(i + 1)));
        page->SetPreferredSize(gfx::Size(kPageWidth, kPageHeight));
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
      page_container_->SetX(-new_selected * kPageWidth);
    }
  }

  void TransitionChanged() override {
    // Update scrolling during the page transition.
    const int origin_x = -model_->selected_page() * kPageWidth;
    const int target_x = -model_->transition().target_page * kPageWidth;
    const double progress = model_->transition().progress;
    page_container_->SetX((1 - progress) * origin_x + progress * target_x);
  }

 private:
  base::raw_ptr<PaginationModel> const model_;
  std::unique_ptr<PaginationController> pagination_controller_;
  base::raw_ptr<PaginationTestContents> page_container_;
  base::ScopedObservation<PaginationModel, PaginationModelObserver>
      model_observer_{this};
};

//------------------------------------------------------------------------------
// PaginationGridView:
class PaginationGridView : public SystemUIComponentsGridView {
 public:
  PaginationGridView()
      : SystemUIComponentsGridView(kGridViewRowNum,
                                   kGridViewColNum,
                                   kGridViewRowGroupSize,
                                   kGirdViewColGroupSize) {}
  PaginationGridView(const PaginationGridView&) = delete;
  PaginationGridView& operator=(const PaginationGridView&) = delete;
  ~PaginationGridView() override = default;

  // Add a pagination instance and a test view binded with the given pagination
  // model.
  void AddPaginationWithModel(
      const std::u16string& name,
      std::unique_ptr<PaginationModel> pagination_model) {
    AddInstance(u"", std::make_unique<PaginationTestScrollView>(
                         pagination_model.get()));
    AddInstance(name, std::make_unique<PaginationView>(pagination_model.get()));
    models_.push_back(std::move(pagination_model));
  }

 private:
  std::vector<std::unique_ptr<PaginationModel>> models_;
};

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreatePaginationInstancesGridView() {
  auto grid_view = std::make_unique<PaginationGridView>();

  // Add a pagination view with 3 pages.
  auto model_three = std::make_unique<PaginationModel>(nullptr);
  model_three->SetTotalPages(3);
  grid_view->AddPaginationWithModel(u"Pagenation with 3 pages",
                                    std::move(model_three));

  // Add a pagination view with 4 pages.
  auto model_five = std::make_unique<PaginationModel>(nullptr);
  model_five->SetTotalPages(5);
  grid_view->AddPaginationWithModel(u"Pagenation with 5 pages",
                                    std::move(model_five));

  // Add a pagination view with 10 pages.
  auto model_ten = std::make_unique<PaginationModel>(nullptr);
  model_ten->SetTotalPages(10);
  grid_view->AddPaginationWithModel(u"Pagenation with 10 pages",
                                    std::move(model_ten));
  return grid_view;
}

}  // namespace ash
