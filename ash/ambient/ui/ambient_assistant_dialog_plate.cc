// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_assistant_dialog_plate.h"

#include <memory>

#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/dialog_plate/mic_view.h"
#include "ash/assistant/ui/main_stage/assistant_query_view.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

AmbientAssistantDialogPlate::AmbientAssistantDialogPlate(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AmbientViewID::kAmbientAssistantDialogPlate);
  InitLayout();

  assistant_controller_observer_.Add(AssistantController::Get());
  AssistantInteractionController::Get()->GetModel()->AddObserver(this);
}

AmbientAssistantDialogPlate::~AmbientAssistantDialogPlate() {
  if (AssistantInteractionController::Get())
    AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);
}

void AmbientAssistantDialogPlate::OnButtonPressed(AssistantButtonId button_id) {
  delegate_->OnDialogPlateButtonPressed(button_id);
}

void AmbientAssistantDialogPlate::OnAssistantControllerDestroying() {
  AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);
  assistant_controller_observer_.Remove(AssistantController::Get());
}

void AmbientAssistantDialogPlate::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  voice_query_view_->SetQuery(query);
}

void AmbientAssistantDialogPlate::OnPendingQueryChanged(
    const AssistantQuery& query) {
  voice_query_view_->SetQuery(query);
}

void AmbientAssistantDialogPlate::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  // Animated voice input toggle button.
  animated_voice_input_toggle_ = AddChildView(
      std::make_unique<MicView>(this, AssistantButtonId::kVoiceInputToggle));
  animated_voice_input_toggle_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_ACCNAME));

  // Voice input query view.
  voice_query_view_ = AddChildView(std::make_unique<AssistantQueryView>());
}

BEGIN_METADATA(AmbientAssistantDialogPlate, views::View)
END_METADATA

}  // namespace ash
