// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_QUICK_ACTION_ITEM_H_
#define ASH_SYSTEM_PHONEHUB_QUICK_ACTION_ITEM_H_

#include "ash/ash_export.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Label;
}

namespace ash {

// A toggle button with labels used in the quick action view.
class QuickActionItem : public views::View, public views::ButtonListener {
 public:
  class Delegate {
   public:
    // Called when the button of the quick action item is clicked.
    virtual void OnButtonPressed(bool is_now_enabled) = 0;
  };

  // |icon_on| and |icon_off| indicates the icons used in on/off state of the
  // button.
  QuickActionItem(Delegate* delegate,
                  int label_id,
                  const gfx::VectorIcon& icon_on,
                  const gfx::VectorIcon& icon_off);

  // If only one icon is supplied, it will be used in both cases.
  QuickActionItem(Delegate* delegate,
                  int label_id,
                  const gfx::VectorIcon& icon);

  ~QuickActionItem() override;
  QuickActionItem(QuickActionItem&) = delete;
  QuickActionItem operator=(QuickActionItem&) = delete;

  // Set the text of sub-label shown below the label.
  void SetSubLabel(const base::string16& sub_label);

  // Set the tooltip text of the icon button.
  void SetIconTooltip(const base::string16& text);

  // Change the toggled state. If toggled, the background color of the circle
  // will change.
  void SetToggled(bool toggled);
  bool IsToggled() const;

  // Get the title/label text of the item.
  const base::string16& GetItemLabel() const;

  // Set the item to be enabled or disabled. When disabled, the button cannot be
  // clicked and the labels are greyed out.
  void SetEnabled(bool enabled);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  bool HasFocus() const override;
  void RequestFocus() override;
  const char* GetClassName() const override;

  FeaturePodIconButton* icon_button() const { return icon_button_; }

 private:
  Delegate* delegate_ = nullptr;

  // Owned by views hierarchy.
  FeaturePodIconButton* icon_button_ = nullptr;
  const gfx::VectorIcon& icon_on_;
  const gfx::VectorIcon& icon_off_;
  views::Label* label_ = nullptr;
  views::Label* sub_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_QUICK_ACTION_VIEW_H_
