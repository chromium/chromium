// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view.h"

#include <algorithm>
#include <numeric>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The default padding within each grid in grid layout.
constexpr int kGridInnerPadding = 5;
// The default spacing between the row groups in grid layout.
constexpr int kGridRowGroupSpacing = 20;
// The default spacing between the column groups in grid layout.
constexpr int kGridColGroupSpacing = 20;
// The default insets of the grid layout border.
constexpr gfx::Insets kGridBorderInsets(10);

}  // namespace

//------------------------------------------------------------------------------
// SystemUIComponentsGridView::GridLayout:
// GridLayout splits the contents into `row_num` x `col_num` grids. Grids in the
// same row have same height and grids in the same column have same width. The
// rows and columns can be divided into equal sized groups. `row_group_size` and
// `col_group_size` indicate the number of rows and columns in each row and
// column group. If the number of rows and columns cannot be divided by their
// group size, the last group will have the remainders. There is a spacing
// between the row and column groups. There is also a border around the grids.
// An example is shown below:
// +---------------------------------------------------------------------------+
// |                                border                                     |
// |        +--------+-------+                   +---------+----------+        |
// |        |        |       | col group spacing |         |          |        |
// |        +--------+-------+                   +---------+----------+        |
// |         row group spacing                     row group spacing           |
// |        +--------+-------+                   +---------+----------+        |
// | border |        |       |                   |         |          | border |
// |        |        |       | col group spacing |         |          |        |
// |        +--------+-------+                   +---------+----------+        |
// |                                border                                     |
// +---------------------------------------------------------------------------+

class SystemUIComponentsGridView::GridLayout : public views::LayoutManagerBase {
 public:
  GridLayout(size_t row_num,
             size_t col_num,
             size_t row_group_size,
             size_t col_group_size,
             int inner_padding,
             int row_group_spacing,
             int col_group_spacing,
             const gfx::Insets& border_insets)
      : row_num_(row_num),
        col_num_(col_num),
        col_width_(col_num),
        row_height_(row_num),
        row_group_size_(row_group_size),
        col_group_size_(col_group_size),
        inner_padding_(inner_padding),
        row_group_spacing_(row_group_spacing),
        col_group_spacing_(col_group_spacing),
        border_insets_(border_insets) {
    // Clamp the row and column group size between 1 and the number of rows and
    // columns when the layout is not empty.
    if (row_num_ > 0 && col_num_ > 0) {
      row_group_size_ =
          std::clamp(row_group_size, static_cast<size_t>(1), row_num_);
      col_group_size_ =
          std::clamp(col_group_size, static_cast<size_t>(1), col_num_);
    }
  }
  GridLayout(const GridLayout&) = delete;
  GridLayout& operator=(const GridLayout&) = delete;
  ~GridLayout() override = default;

  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    views::ProposedLayout layout;
    // No layout if either row/column is empty.
    if (row_num_ == 0 || col_num_ == 0)
      return layout;

    // The x of grids origin in different columns.
    std::vector<int> ori_x(col_num_, 0);
    // The y of grids origin in different rows.
    std::vector<int> ori_y(row_num_, 0);

    ori_x[0] = border_insets_.left();
    ori_y[0] = border_insets_.top();
    for (size_t i = 0; i < children_.size(); i++) {
      int row_index = i / col_num_;
      int col_index = i % col_num_;
      // Calculate the origin posisitons.
      if (row_index == 0 && col_index > 0) {
        int col_padding =
            (col_index % col_group_size_) ? 0 : col_group_spacing_;
        ori_x[col_index] =
            ori_x[col_index - 1] + col_width_[col_index - 1] + col_padding;
      }
      if (row_index > 0 && col_index == 0) {
        int row_padding =
            (row_index % row_group_size_) ? 0 : row_group_spacing_;
        ori_y[row_index] =
            ori_y[row_index - 1] + row_height_[row_index - 1] + row_padding;
      }

      // Skip empty instances.
      if (!children_[i])
        continue;

      layout.child_layouts.emplace_back(children_[i], true);
      views::ChildLayout& child_layout = layout.child_layouts.back();

      // Put the view in the center of the grid.
      int view_width = children_[i]->GetPreferredSize({}).width();
      int view_height = children_[i]->GetPreferredSize({}).height();

      child_layout.bounds = gfx::Rect(
          ori_x[col_index] + inner_padding_,
          ori_y[row_index] + (row_height_[row_index] - view_height) / 2,
          view_width, view_height);
    }

    int width = std::reduce(col_width_.begin(), col_width_.end(), 0) +
                (col_num_ - 1) / col_group_size_ * col_group_spacing_ +
                border_insets_.width();
    int height = std::reduce(row_height_.begin(), row_height_.end(), 0) +
                 (row_num_ - 1) / row_group_size_ * row_group_spacing_ +
                 border_insets_.height();
    layout.host_size = gfx::Size(width, height);

    return layout;
  }

  // Append a view (or nullptr) in `children_`.
  void AppendView(views::View* host, views::View* view) {
    // Number of children cannot exceed the layout capacity.
    DCHECK_LT(children_.size(), row_num_ * col_num_);
    children_.emplace_back(view);
    if (view)
      ChildViewSizeChanged(host, view);
  }

  bool OnViewRemoved(View* host, View* view) override {
    auto iter =
        std::find_if(children_.begin(), children_.end(),
                     [view](views::View* child) { return view == child; });
    DCHECK(iter != children_.end());
    *iter = nullptr;

    return views::LayoutManagerBase::OnViewRemoved(host, view);
  }

  void ChildPreferredSizeChanged(views::View* host, views::View* view) {
    ChildViewSizeChanged(host, view);
  }

 private:
  // Called when the size of a `view` in `children_` changed.
  void ChildViewSizeChanged(views::View* host, views::View* view) {
    DCHECK(view);

    // Get the index of `view` in `children_`.
    auto iter = base::ranges::find(children_, view);
    DCHECK(iter != children_.end());
    const int view_index = std::distance(children_.begin(), iter);

    // When a view size is changed, updates the max width of the column and max
    // height of the row.
    int row_index = view_index / col_num_;
    int col_index = view_index % col_num_;

    for (size_t i = 0; i < col_num_; i++) {
      const size_t index = row_index * col_num_ + i;
      if (index >= children_.size()) {
        break;
      }

      const auto* child = children_[index].get();
      if (child) {
        row_height_[row_index] =
            std::max(row_height_[row_index],
                     child->GetPreferredSize().height() + 2 * inner_padding_);
      }
    }

    for (size_t i = 0; i < row_num_; i++) {
      const size_t index = i * col_num_ + col_index;
      if (index >= children_.size()) {
        break;
      }

      const auto* child = children_[index].get();
      if (child) {
        col_width_[col_index] =
            std::max(col_width_[col_index],
                     child->GetPreferredSize().width() + 2 * inner_padding_);
      }
    }

    // Re-layout the host view.
    InvalidateHost(true);
  }

  // The number of rows and columns.
  const size_t row_num_;
  const size_t col_num_;
  // The width of different columns.
  std::vector<int> col_width_;
  // The height of different rows.
  std::vector<int> row_height_;
  // The size of each row and column group.
  size_t row_group_size_;
  size_t col_group_size_;
  // The padding in each grid.
  int inner_padding_;
  // Spacing between row groups.
  int row_group_spacing_;
  // Spacing between column groups.
  int col_group_spacing_;
  gfx::Insets border_insets_;

  std::vector<raw_ptr<views::View, VectorExperimental>> children_;
};

// -----------------------------------------------------------------------------
// SystemUIComponentsGridView:
// We assume each column in the contents view at least has a label and
// an instance. Therefore, the column of contents view occupies two columns of
// the grid layout.
SystemUIComponentsGridView::SystemUIComponentsGridView(size_t row_num,
                                                       size_t col_num,
                                                       size_t row_group_size,
                                                       size_t col_group_size)
    : grid_layout_(
          SetLayoutManager(std::make_unique<GridLayout>(row_num,
                                                        2 * col_num,
                                                        row_group_size,
                                                        2 * col_group_size,
                                                        kGridInnerPadding,
                                                        kGridRowGroupSpacing,
                                                        kGridColGroupSpacing,
                                                        kGridBorderInsets))) {}

SystemUIComponentsGridView::~SystemUIComponentsGridView() = default;

void SystemUIComponentsGridView::ChildPreferredSizeChanged(views::View* child) {
  // Update the layout when a child size is changed.
  grid_layout_->ChildPreferredSizeChanged(this, child);
  PreferredSizeChanged();
}

void SystemUIComponentsGridView::AddInstanceImpl(
    const std::u16string& name,
    std::unique_ptr<views::View> instance_view) {
  views::Label* label_ptr = nullptr;
  views::View* instance_ptr = instance_view.get();
  if (instance_view) {
    // Add a label and an instance in the contents.
    auto label = std::make_unique<views::Label>(name);
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    label_ptr = AddChildView(std::move(label));
    AddChildView(std::move(instance_view));
  }

  grid_layout_->AppendView(this, label_ptr);
  grid_layout_->AppendView(this, instance_ptr);
  PreferredSizeChanged();
}

}  // namespace ash
