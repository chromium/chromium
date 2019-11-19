// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_

#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class StyledLabel;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

// AssistantOptInView ----------------------------------------------------------

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantOptInView
    : public views::View,
      public views::ButtonListener,
      public AssistantStateObserver {
 public:
  explicit AssistantOptInView(AssistantViewDelegate* delegate_);
  ~AssistantOptInView() override;

  // views::View:
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // AssistantStateObserver:
  void OnAssistantConsentStatusChanged(int consent_status) override;

 private:
  void InitLayout();
  void UpdateLabel(int consent_status);

  views::StyledLabel* label_;  // Owned by view hierarchy.

  views::Button* container_;

  AssistantViewDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AssistantOptInView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_
