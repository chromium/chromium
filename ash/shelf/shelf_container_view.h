// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTAINER_VIEW_H_
#define ASH_SHELF_SHELF_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class ASH_EXPORT ShelfContainerView : public views::View {
  METADATA_HEADER(ShelfContainerView, views::View)

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
  // TODO(crbug.com/40631809): now we implement ShelfView scrolling
  // through view translation, which is not as efficient as ScrollView. Redesign
  // this class with ScrollView.
  virtual void TranslateShelfView(const gfx::Vector2dF& offset);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void ChildPreferredSizeChanged(views::View* child) override;

 protected:
  // Owned by views hierarchy.
  raw_ptr<ShelfView> shelf_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTAINER_VIEW_H_
