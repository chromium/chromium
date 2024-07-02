// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_DIALOG_PLATE_MIC_VIEW_H_
#define ASH_ASSISTANT_UI_DIALOG_PLATE_MIC_VIEW_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/ui/base/assistant_button.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class LogoView;

// A stateful view belonging to DialogPlate which indicates current mic state
// and delivers notification of press events.
class COMPONENT_EXPORT(ASSISTANT_UI) MicView
    : public AssistantButton,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver {
  METADATA_HEADER(MicView, AssistantButton)

 public:
  MicView(AssistantButtonListener* listener,
          AssistantButtonId button_id);
  MicView(const MicView&) = delete;
  MicView& operator=(const MicView&) = delete;
  ~MicView() override;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

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

  raw_ptr<LogoView> logo_view_;  // Owned by view hierarchy.

  // True when speech level goes above a threshold and sets LogoView in
  // kUserSpeaks state.
  bool is_user_speaking_ = false;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_DIALOG_PLATE_MIC_VIEW_H_
