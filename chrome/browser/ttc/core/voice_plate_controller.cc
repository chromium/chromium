// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/voice_plate_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/ttc/ttc_voice_plate.h"
#include "ui/views/view.h"

namespace ttc {

using voice::WaveformMode;

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
  MoveToWindow(browsers->FindTabbedBrowser());
}

VoicePlateController::~VoicePlateController() = default;

void VoicePlateController::SetMode(WaveformMode mode) {
  if (voice_plate_) {
    voice_plate_->SetMode(mode);
  }
}

void VoicePlateController::UpdateAudioLevel(float audio_level) {
  if (voice_plate_) {
    voice_plate_->UpdateAudioLevel(audio_level);
  }
}

void VoicePlateController::OnBrowserActivated(BrowserWindowInterface* browser) {
  // TTC follows the user's tabbed window, which supplies page context and
  // tools, so app and popup windows don't take the plate.
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return;
  }
  MoveToWindow(browser);
}

void VoicePlateController::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (browser != anchored_browser_) {
    return;
  }

  // A normal exit marks every window delete-scheduled before any of them
  // closes, so FindTabbedBrowser() skips them. Other shutdown paths, such as
  // ChromeOS session end, may not, so also skip the move once shutdown has
  // started; otherwise each closing window would get a new plate.
  if (!browser_shutdown::HasShutdownStarted()) {
    MoveToWindow(
        browser_collection_observation_.GetSource()->FindTabbedBrowser());
  }
  if (anchored_browser_ != browser) {
    return;
  }

  // The plate is recreated when a tabbed window is next activated.
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

  WaveformMode initial_mode = WaveformMode::kProcessing;
  if (voice_plate_) {
    initial_mode = voice_plate_->mode();
  }

  anchored_browser_ = browser;

  // Re-parenting the existing View turned out to be somewhat fraught, at least
  // on Linux, so we recreate the View on each window move.
  voice_plate_ = std::make_unique<TtcVoicePlate>(anchor_view, close_callback_);
  voice_plate_->SetMode(initial_mode);
  voice_plate_->Show();
}

}  // namespace ttc
