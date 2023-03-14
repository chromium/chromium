// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_BOTTOM_STATUS_INDICATOR_H_
#define ASH_LOGIN_UI_BOTTOM_STATUS_INDICATOR_H_

#include "ash/style/ash_color_id.h"
#include "base/memory/weak_ptr.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {

class BottomStatusIndicator
    : public views::LabelButton,
      public base::SupportsWeakPtr<BottomStatusIndicator> {
 public:
  using TappedCallback = base::RepeatingClosure;

  explicit BottomStatusIndicator(TappedCallback on_tapped_callback);
  BottomStatusIndicator(const BottomStatusIndicator&) = delete;
  BottomStatusIndicator& operator=(const BottomStatusIndicator&) = delete;
  ~BottomStatusIndicator() override;

  void SetIcon(const gfx::VectorIcon& vector_icon, ui::ColorId color_id);

  void set_role_for_accessibility(ax::mojom::Role role) { role_ = role; }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  ax::mojom::Role role_ = ax::mojom::Role::kStaticText;
};

}  // namespace ash
#endif  // ASH_LOGIN_UI_BOTTOM_STATUS_INDICATOR_H_
