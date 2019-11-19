// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace gfx {
class SlideAnimation;
}

namespace views {
class ImageView;
}

namespace ash {
class Shelf;

// Label view which can be given a different data from the visible label.
// IME icons like "US" (US keyboard) or "あ(Google Japanese Input)" are
// rendered as a label, but reading such text literally will not always be
// understandable.
class IconizedLabel : public views::Label {
 public:
  void SetCustomAccessibleName(const base::string16& name) {
    custom_accessible_name_ = name;
  }

  // views::Label:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  base::string16 custom_accessible_name_;
};

// Base-class for items in the tray. It makes sure the widget is updated
// correctly when the visibility/size of the tray item changes. It also adds
// animation when showing/hiding the item in the tray.
class ASH_EXPORT TrayItemView : public views::View,
                                public views::AnimationDelegateViews {
 public:
  explicit TrayItemView(Shelf* shelf);
  ~TrayItemView() override;

  // Convenience function for creating a child Label or ImageView.
  // Only one of the two should be called.
  void CreateLabel();
  void CreateImageView();

  IconizedLabel* label() const { return label_; }
  views::ImageView* image_view() const { return image_view_; }

  // views::View.
  void SetVisible(bool visible) override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  const char* GetClassName() const override;

 protected:
  // Returns whether the shelf is horizontal.
  bool IsHorizontalAlignment() const;

 private:
  // views::View.
  void ChildPreferredSizeChanged(View* child) override;

  // views::AnimationDelegateViews.
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  Shelf* const shelf_;
  std::unique_ptr<gfx::SlideAnimation> animation_;
  // Only one of |label_| and |image_view_| should be non-null.
  IconizedLabel* label_;
  views::ImageView* image_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayItemView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
