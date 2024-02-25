// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SCROLL_ARROW_VIEW_H_
#define ASH_SHELF_SCROLL_ARROW_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {
class Shelf;
class ShelfButtonDelegate;

class ASH_EXPORT ScrollArrowView : public ShelfButton {
  METADATA_HEADER(ScrollArrowView, ShelfButton)

 public:
  enum ArrowType { kLeft, kRight };
  ScrollArrowView(ArrowType arrow_type,
                  bool is_horizontal_alignment,
                  Shelf* shelf,
                  ShelfButtonDelegate* button_listener);

  ScrollArrowView(const ScrollArrowView&) = delete;
  ScrollArrowView& operator=(const ScrollArrowView&) = delete;

  ~ScrollArrowView() override;

  void set_is_horizontal_alignment(bool is_horizontal_alignment) {
    is_horizontal_alignment_ = is_horizontal_alignment;
  }

  // views::Button:
  void NotifyClick(const ui::Event& event) override;

  // views::View:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  ArrowType arrow_type_ = kLeft;
  bool is_horizontal_alignment_ = true;
};

}  // namespace ash

#endif  // ASH_SHELF_SCROLL_ARROW_VIEW_H_
