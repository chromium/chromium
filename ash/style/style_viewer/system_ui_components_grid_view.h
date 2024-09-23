// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_STYLE_VIEWER_SYSTEM_UI_COMPONENTS_GRID_VIEW_H_
#define ASH_STYLE_STYLE_VIEWER_SYSTEM_UI_COMPONENTS_GRID_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace ash {

// `SystemUIComponentsGridView` is the view to present a grid of instances of a
// system UI component. Each instance has one of the types and states of the
// component. Instances and corresponding names are placed in a `row_num` x
// `col_num` grid layout in the row-major order as below:
// +---------------------------------+---------------------------------+
// | name of instance 1   instance 1 | name of instance 2   instance 2 |
// +---------------------------------+---------------------------------+
// | name of instance 3   instance 3 | name of instance 4   instance 4 |
// +---------------------------------+---------------------------------+
//
// We can also split the rows and columns in equal sized groups.
// `row_group_size` and `col_group_size` indicate the number of rows and columns
// in each group. If the number of rows and columns cannot be divided by their
// group size, we would fill the frontmost groups and leave the last group with
// the remainder. Please refer to the implementation of
// `SystemUIComponentsGridView::GridLayout` for more details.
class SystemUIComponentsGridView : public views::View {
 public:
  SystemUIComponentsGridView(size_t row_num,
                             size_t col_num,
                             size_t row_group_size,
                             size_t col_group_size);
  SystemUIComponentsGridView(const SystemUIComponentsGridView&) = delete;
  SystemUIComponentsGridView& operator=(const SystemUIComponentsGridView&) =
      delete;
  ~SystemUIComponentsGridView() override;

  // Adds a new instance and returns the raw pointer of the instance.
  template <typename T>
  T* AddInstance(const std::u16string& name, std::unique_ptr<T> instance) {
    T* instance_ptr = instance.get();
    AddInstanceImpl(name, std::move(instance));
    return instance_ptr;
  }

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  class GridLayout;

  // Adds the view of the instance and label in the view's hierarchy.
  void AddInstanceImpl(const std::u16string& name,
                       std::unique_ptr<views::View> instance_view);

  // The grid layout holding the labels and instances.
  raw_ptr<GridLayout> grid_layout_;
};

}  // namespace ash

#endif  // ASH_STYLE_STYLE_VIEWER_SYSTEM_UI_COMPONENTS_GRID_VIEW_H_
