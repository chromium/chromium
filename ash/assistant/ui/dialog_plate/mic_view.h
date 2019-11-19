// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_DIALOG_PLATE_MIC_VIEW_H_
#define ASH_ASSISTANT_UI_DIALOG_PLATE_MIC_VIEW_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/ui/base/assistant_button.h"
#include "base/component_export.h"
#include "base/macros.h"

namespace ash {

enum class AssistantButtonId;
class AssistantViewDelegate;
class LogoView;

// A stateful view belonging to DialogPlate which indicates current mic state
// and delivers notification of press events.
class COMPONENT_EXPORT(ASSISTANT_UI) MicView
    : public AssistantButton,
      public AssistantInteractionModelObserver {
 public:
  MicView(views::ButtonListener* listener,
          AssistantViewDelegate* delegate,
          AssistantButtonId button_id);
  ~MicView() override;

  // AssistantButton:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;

  // AssistantInteractionModelObserver:
  void OnMicStateChanged(MicState mic_state) override;
  void OnSpeechLevelChanged(float speech_level_db) override;

 private:
  void InitLayout();

  // If |animate| is false, there is no exit animation of current state and
  // enter animation of the next state of the LogoView. Note that |animate| will
  // only take effect if Assistant UI is visible. Otherwise, we proceed
  // immediately to the next state regardless of |animate|.
  void UpdateState(bool animate);

  AssistantViewDelegate* const delegate_;

  LogoView* logo_view_;  // Owned by view hierarchy.

  // True when speech level goes above a threshold and sets LogoView in
  // kUserSpeaks state.
  bool is_user_speaking_ = false;

  DISALLOW_COPY_AND_ASSIGN(MicView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_DIALOG_PLATE_MIC_VIEW_H_
