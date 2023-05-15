// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTAINER_VIEW_H_
#define ASH_SHELF_SHELF_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace ash {

class ASH_EXPORT ShelfContainerView : public views::View {
 public:
  explicit ShelfContainerView(ShelfView* shelf_view);

  ShelfContainerView(const ShelfContainerView&) = delete;
  ShelfContainerView& operator=(const ShelfContainerView&) = delete;

  ~ShelfContainerView() override;

  void Initialize();

  // Calculates the ideal size of |shelf_view_| to accommodate all of app
  // buttons without scrolling.
  gfx::Size CalculateIdealSize(int button_size) const;

  // Translate |shelf_view_| by |offset|.
  // TODO(https://crbug.com/973481): now we implement ShelfView scrolling
  // through view translation, which is not as efficient as ScrollView. Redesign
  // this class with ScrollView.
  virtual void TranslateShelfView(const gfx::Vector2dF& offset);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  const char* GetClassName() const override;

 protected:
  // Owned by views hierarchy.
  raw_ptr<ShelfView, ExperimentalAsh> shelf_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTAINER_VIEW_H_
