// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/voice_plate_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "ui/views/view.h"

namespace ttc {

namespace {

views::View* GetAnchorView(BrowserWindowInterface* browser) {
  if (!browser) {
    return nullptr;
  }
  auto* browser_elements = BrowserElementsViews::From(browser);
  return browser_elements ? browser_elements->GetView(kTopContainerElementId)
                          : nullptr;
}

}  // namespace

VoicePlateController::VoicePlateController(
    Profile& profile,
    base::RepeatingClosure close_callback)
    : close_callback_(std::move(close_callback)) {
  ProfileBrowserCollection* browsers =
      ProfileBrowserCollection::GetForProfile(&profile);
  if (!browsers) {
    return;
  }

  browser_collection_observation_.Observe(browsers);
  MoveToWindow(browsers->GetLastActiveBrowser());
}

VoicePlateController::~VoicePlateController() = default;

void VoicePlateController::SetState(dictation::UiState state) {
  if (voice_plate_) {
    voice_plate_->SetState(state);
  }
}

void VoicePlateController::UpdateAudioLevel(float audio_level) {
  if (voice_plate_) {
    voice_plate_->UpdateAudioLevel(audio_level);
  }
}

void VoicePlateController::OnBrowserActivated(BrowserWindowInterface* browser) {
  MoveToWindow(browser);
}

void VoicePlateController::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (browser != anchored_browser_) {
    return;
  }

  // The plate is recreated in whichever window is activated next.
  anchored_browser_ = nullptr;
  voice_plate_.reset();
}

void VoicePlateController::MoveToWindow(BrowserWindowInterface* browser) {
  views::View* anchor_view = GetAnchorView(browser);
  if (!anchor_view) {
    return;
  }

  if (browser == anchored_browser_) {
    return;
  }

  dictation::UiState initial_state = dictation::UiState::kInitializing;
  if (voice_plate_) {
    initial_state = voice_plate_->state();
  }

  anchored_browser_ = browser;

  // Re-parenting the existing View turned out to be somewhat fraught, at least
  // on Linux, so we recreate the View on each window move.
  voice_plate_ = std::make_unique<dictation::DictationBubbleUi>(
      anchor_view, close_callback_,
      /*toggle_active_stream_callback=*/base::DoNothing());
  voice_plate_->SetState(initial_state);
  voice_plate_->Show();
}

}  // namespace ttc
