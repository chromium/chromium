// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_QUICK_ACTIONS_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_QUICK_ACTIONS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

namespace ash {

// A toggle button with labels used in the quick actions view.
class QuickActionsItem : public views::View {
 public:
  explicit QuickActionsItem(views::ButtonListener* listener,
                            const gfx::VectorIcon& vector_icon,
                            int label_id);
  ~QuickActionsItem() override;
  QuickActionsItem(QuickActionsItem&) = delete;
  QuickActionsItem operator=(QuickActionsItem&) = delete;

  // Set the text of sub-label shown below the label.
  void SetSubLabel(const base::string16& sub_label);

  // Set the tooltip text of the icon button.
  void SetIconTooltip(const base::string16& text);

  // Change the toggled state. If toggled, the background color of the circle
  // will change.
  void SetToggled(bool toggled);
  bool IsToggled() const { return icon_button_->toggled(); }

  // Get the title/label text of the item.
  const base::string16& GetItemLabel() const;

  // views::View:
  bool HasFocus() const override;
  void RequestFocus() override;
  const char* GetClassName() const override;

  FeaturePodIconButton* icon_button() const { return icon_button_; }

 private:
  // Owned by views hierarchy.
  FeaturePodIconButton* icon_button_ = nullptr;
  views::Label* label_ = nullptr;
  views::Label* sub_label_ = nullptr;
};

// A view in Phone Hub bubble that contains toggle button for quick actions such
// as enable hotspot, silence phone and locate phone.
class ASH_EXPORT QuickActionsView : public views::View,
                                    public views::ButtonListener {
 public:
  QuickActionsView();
  ~QuickActionsView() override;
  QuickActionsView(QuickActionsView&) = delete;
  QuickActionsView operator=(QuickActionsView&) = delete;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void Update();
  void UpdateItem(QuickActionsItem* item, bool is_enabled);

  QuickActionsItem* enable_hotspot_ = nullptr;
  QuickActionsItem* silence_phone_ = nullptr;
  QuickActionsItem* locate_phone_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_QUICK_ACTIONS_VIEW_H_
