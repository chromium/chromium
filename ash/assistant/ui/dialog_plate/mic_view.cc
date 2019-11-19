// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate/mic_view.h"

#include <memory>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/logo_view/logo_view.h"
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

MicView::MicView(views::ButtonListener* listener,
                 AssistantViewDelegate* delegate,
                 AssistantButtonId button_id)
    : AssistantButton(listener, button_id), delegate_(delegate) {
  InitLayout();

  // The AssistantViewDelegate is owned by AssistantController which is
  // guaranteed to outlive the Assistant view hierarchy.
  delegate_->AddInteractionModelObserver(this);
}

MicView::~MicView() {
  delegate_->RemoveInteractionModelObserver(this);
}

const char* MicView::GetClassName() const {
  return "MicView";
}

gfx::Size MicView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredSizeDip, GetHeightForWidth(kPreferredSizeDip));
}

int MicView::GetHeightForWidth(int width) const {
  return kPreferredSizeDip;
}

void MicView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Logo view container.
  views::View* logo_view_container = new views::View();
  logo_view_container->set_can_process_events_within_subtree(false);
  AddChildView(logo_view_container);

  views::BoxLayout* layout_manager =
      logo_view_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  // Logo view.
  logo_view_ = LogoView::Create();
  logo_view_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  logo_view_container->AddChildView(logo_view_);

  // Initialize state.
  UpdateState(/*animate=*/false);
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

void MicView::UpdateState(bool animate) {
  const AssistantInteractionModel* interaction_model =
      delegate_->GetInteractionModel();

  if (animate) {
    // If Assistant UI is not visible, we shouldn't attempt to animate state
    // changes. We should instead advance immediately to the next state.
    const AssistantUiModel* ui_model = delegate_->GetUiModel();
    animate = ui_model->visibility() == AssistantVisibility::kVisible;
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

}  // namespace ash
