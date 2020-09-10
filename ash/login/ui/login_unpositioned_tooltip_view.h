// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_UNPOSITIONED_TOOLTIP_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_UNPOSITIONED_TOOLTIP_VIEW_H_

#include "ash/login/ui/login_base_bubble_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// TODO(crbug.com/1109266): Get rid of this class and make
// LoginBaseBubbleView more configurable.
class LoginUnpositionedTooltipView : public LoginBaseBubbleView {
 public:
  LoginUnpositionedTooltipView(const base::string16& message,
                               views::View* anchor_view);
  ~LoginUnpositionedTooltipView() override;

  void SetText(const base::string16& message);

  // LoginBaseBubbleView:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 protected:
  views::Label* label() { return label_; }

 private:
  views::Label* label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginUnpositionedTooltipView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_UNPOSITIONED_TOOLTIP_VIEW_H_
