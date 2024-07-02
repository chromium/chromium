// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate/mic_view.h"

#include <memory>

#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/logo_view/logo_view.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
// The desired height for the LogoView icon is 24dip in mic state to match the
// static mic button in DialogPlate. The |kMicIcon| resource used for the static
// button has different internal padding than does that of the icon drawn by
// LogoView, so we add 2dip for visual consistency.
constexpr int kIconSizeDip = 26;
constexpr int kPreferredSizeDip = 32;

}  // namespace

MicView::MicView(AssistantButtonListener* listener, AssistantButtonId button_id)
    : AssistantButton(listener, button_id) {
  InitLayout();

  assistant_controller_observation_.Observe(AssistantController::Get());
  AssistantInteractionController::Get()->GetModel()->AddObserver(this);
}

MicView::~MicView() {
  if (AssistantInteractionController::Get())
    AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);
}

gfx::Size MicView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kPreferredSizeDip, kPreferredSizeDip);
}

void MicView::OnAssistantControllerDestroying() {
  AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);
  DCHECK(assistant_controller_observation_.IsObservingSource(
      AssistantController::Get()));
  assistant_controller_observation_.Reset();
}

void MicView::OnMicStateChanged(MicState mic_state) {
  is_user_speaking_ = false;
  UpdateState(/*animate=*/true);
}

void MicView::OnSpeechLevelChanged(float speech_level_db) {
  // TODO: Work with UX to determine the threshold.
  constexpr float kSpeechLevelThreshold = -60.0f;
  if (speech_level_db < kSpeechLevelThreshold)
    return;

  logo_view_->SetSpeechLevel(speech_level_db);
  if (!is_user_speaking_) {
    is_user_speaking_ = true;
    UpdateState(/*animate=*/true);
  }
}

void MicView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Logo view container.
  auto logo_view_container = std::make_unique<views::View>();
  logo_view_container->SetCanProcessEventsWithinSubtree(false);

  views::BoxLayout* layout_manager =
      logo_view_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  // Logo view.
  logo_view_ = logo_view_container->AddChildView(LogoView::Create());
  logo_view_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));

  AddChildView(std::move(logo_view_container));

  // Initialize state.
  UpdateState(/*animate=*/false);
}

void MicView::UpdateState(bool animate) {
  const AssistantInteractionModel* interaction_model =
      AssistantInteractionController::Get()->GetModel();

  if (animate) {
    // If Assistant UI is not visible, we shouldn't attempt to animate state
    // changes. We should instead advance immediately to the next state.
    animate = AssistantUiController::Get()->GetModel()->visibility() ==
              AssistantVisibility::kVisible;
  }

  LogoView::State mic_state;
  switch (interaction_model->mic_state()) {
    case MicState::kClosed:
      mic_state = LogoView::State::kMic;
      break;
    case MicState::kOpen:
      mic_state = is_user_speaking_ ? LogoView::State::kUserSpeaks
                                    : LogoView::State::kListening;
      break;
  }
  logo_view_->SetState(mic_state, animate);
}

BEGIN_METADATA(MicView)
END_METADATA

}  // namespace ash
