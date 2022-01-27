// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVE_DESK_TEMPLATE_BUTTON_H_
#define ASH_WM_DESKS_TEMPLATES_SAVE_DESK_TEMPLATE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/style/pill_button.h"
#include "ash/wm/overview/overview_highlightable_view.h"
#include "base/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class ASH_EXPORT SaveDeskTemplateButton : public PillButton,
                                          public OverviewHighlightableView {
 public:
  METADATA_HEADER(SaveDeskTemplateButton);

  explicit SaveDeskTemplateButton(base::RepeatingClosure callback);
  SaveDeskTemplateButton(const SaveDeskTemplateButton&) = delete;
  SaveDeskTemplateButton& operator=(const SaveDeskTemplateButton&) = delete;
  ~SaveDeskTemplateButton() override;

 private:
  // OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  void MaybeSwapHighlightedView(bool right) override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

  void UpdateBorderState();

  base::RepeatingClosure callback_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVE_DESK_TEMPLATE_BUTTON_H_
