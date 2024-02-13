// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_QUICK_ACTION_ITEM_H_
#define ASH_SYSTEM_PHONEHUB_QUICK_ACTION_ITEM_H_

#include "ash/ash_export.h"
#include "ash/system/unified/feature_pod_button.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}

namespace ash {

// A toggle button with labels used in the quick action view.
class ASH_EXPORT QuickActionItem : public views::View {
  METADATA_HEADER(QuickActionItem, views::View)

 public:
  class Delegate {
   public:
    // Called when the button of the quick action item is clicked.
    virtual void OnButtonPressed(bool is_now_enabled) = 0;
  };

  // If only one icon is supplied, it will be used in both cases.
  QuickActionItem(Delegate* delegate,
                  int label_id,
                  const gfx::VectorIcon& icon);

  ~QuickActionItem() override;
  QuickActionItem(QuickActionItem&) = delete;
  QuickActionItem operator=(QuickActionItem&) = delete;

  // Set the text of sub-label shown below the label.
  void SetSubLabel(const std::u16string& sub_label);

  // Set the color of sub-label shown below the label.
  void SetSubLabelColor(SkColor color);

  // Set the tooltip text of the icon button.
  void SetTooltip(const std::u16string& text);

  // Change the toggled state. If toggled, the background color of the circle
  // will change.
  void SetToggled(bool toggled);
  bool IsToggled() const;

  // Get the title/label text of the item.
  const std::u16string& GetItemLabel() const;

  // Set the item to be enabled or disabled. When disabled, the button cannot be
  // clicked and the labels are greyed out.
  void SetEnabled(bool enabled);

  // views::View:
  bool HasFocus() const override;
  void RequestFocus() override;

  FeaturePodIconButton* icon_button() const { return icon_button_; }

 private:
  // Owned by views hierarchy.
  raw_ptr<FeaturePodIconButton> icon_button_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::Label> sub_label_ = nullptr;

  // Enabled color of the sub label.
  SkColor sub_label_color_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_QUICK_ACTION_ITEM_H_
