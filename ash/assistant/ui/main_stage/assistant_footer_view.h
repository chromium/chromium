// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_FOOTER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_FOOTER_VIEW_H_

#include <memory>
#include <string>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ui {
class CallbackLayerAnimationObserver;
}  // namespace ui

namespace ash {

class AssistantOptInView;
class AssistantViewDelegate;
class SuggestionContainerView;

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantFooterView
    : public views::View,
      public AssistantStateObserver {
 public:
  explicit AssistantFooterView(AssistantViewDelegate* delegate);
  ~AssistantFooterView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;

  // AssistantStateObserver:
  void OnAssistantConsentStatusChanged(int consent_status) override;

 private:
  void InitLayout();

  void OnAnimationStarted(const ui::CallbackLayerAnimationObserver& observer);
  bool OnAnimationEnded(const ui::CallbackLayerAnimationObserver& observer);

  AssistantViewDelegate* const delegate_;  // Owned by Shell.

  SuggestionContainerView* suggestion_container_;  // Owned by view hierarchy.
  AssistantOptInView* opt_in_view_;                // Owned by view hierarchy.

  std::unique_ptr<ui::CallbackLayerAnimationObserver> animation_observer_;

  DISALLOW_COPY_AND_ASSIGN(AssistantFooterView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_FOOTER_VIEW_H_
