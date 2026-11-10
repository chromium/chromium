// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/overlay_controller.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/ttc/ttc_view.h"
#include "chrome/browser/ui/views/ttc/ttc_view_delegate.h"
#include "chrome/browser/ui/views/ttc/ttc_voice_plate.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace ttc {

namespace {

// Whether fullscreen hides the top container the plate is anchored to. Matches
// ToastController::ShouldRenderToastOverWebContents().
bool ShouldRenderOverWebContents(const BrowserWindowInterface* browser) {
  bool render_over_web_contents =
      BrowserWindowFullscreenController::From(browser)
          ->ShouldHideUIForFullscreen();
#if BUILDFLAG(IS_MAC)
  // Mac uses immersive mode for all fullscreen, so ShouldHideUIForFullscreen()
  // is false in tab fullscreen even though the toolbar is hidden.
  render_over_web_contents |= fullscreen_utils::IsInContentFullscreen(browser);
#endif  // BUILDFLAG(IS_MAC)
  return render_over_web_contents;
}

views::View* GetAnchorView(BrowserWindowInterface* browser) {
  if (!browser) {
    return nullptr;
  }
  auto* browser_elements = BrowserElementsViews::From(browser);
  return browser_elements ? browser_elements->GetView(kTopContainerElementId)
                          : nullptr;
}

}  // namespace

OverlayController::OverlayController(Profile& profile,
                                     TtcViewDelegate& delegate)
    : delegate_(delegate) {
  ProfileBrowserCollection* browsers =
      ProfileBrowserCollection::GetForProfile(&profile);
  if (!browsers) {
    return;
  }

  browser_collection_observation_.Observe(browsers);
  MoveToWindow(browsers->FindTabbedBrowser());
}

OverlayController::~OverlayController() = default;

void OverlayController::ApplyUiState(UiState ui_state) {
  ui_state_ = ui_state;
  for (const std::unique_ptr<TtcView>& view : views_) {
    view->ApplyUiState(ui_state);
  }
}

void OverlayController::OnBrowserActivated(BrowserWindowInterface* browser) {
  // TTC follows the user's tabbed window, which supplies page context and
  // tools, so app and popup windows don't take the views.
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return;
  }
  MoveToWindow(browser);
}

void OverlayController::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (browser != anchored_browser_) {
    return;
  }

  // A normal exit marks every window delete-scheduled before any of them
  // closes, so FindTabbedBrowser() skips them. Other shutdown paths, such as
  // ChromeOS session end, may not, so also skip the move once shutdown has
  // started; otherwise each closing window would get new views.
  const bool shutting_down = browser_shutdown::HasShutdownStarted();
  if (!shutting_down) {
    MoveToWindow(
        browser_collection_observation_.GetSource()->FindTabbedBrowser());
    if (anchored_browser_ != browser) {
      return;
    }
  }

  fullscreen_subscription_ = {};
  anchored_browser_ = nullptr;
  voice_plate_ = nullptr;
  views_.clear();

  // No tabbed window is left to show the views in or to supply page context,
  // so end the session rather than keep listening with no UI.
  if (!shutting_down) {
    delegate_->EndSessionAsync();
  }
}

void OverlayController::MoveToWindow(BrowserWindowInterface* browser) {
  views::View* anchor_view = GetAnchorView(browser);
  if (!anchor_view) {
    return;
  }

  if (browser == anchored_browser_) {
    return;
  }

  fullscreen_subscription_ = {};
  anchored_browser_ = browser;

  // Re-parenting the existing views turned out to be somewhat fraught, at least
  // on Linux, so we recreate the views on each window move.
  voice_plate_ = nullptr;
  views_.clear();
  CreateViews(anchor_view);

  fullscreen_subscription_ =
      ExclusiveAccessManager::From(browser)
          ->fullscreen_controller()
          ->RegisterOnFullscreenStateChanged(base::BindRepeating(
              &OverlayController::OnFullscreenStateChanged,
              base::Unretained(this)));
}

void OverlayController::CreateViews(views::View* anchor_view) {
  CHECK(views_.empty());
  // The views are recreated on every window move, so they can't keep state of
  // their own.
  auto voice_plate = std::make_unique<TtcVoicePlate>(anchor_view, *delegate_);
  voice_plate->SetRenderOverWebContents(
      ShouldRenderOverWebContents(anchored_browser_));
  voice_plate_ = voice_plate.get();
  views_.push_back(std::move(voice_plate));

  for (const std::unique_ptr<TtcView>& view : views_) {
    view->ApplyUiState(ui_state_);
    view->Show();
  }
}

void OverlayController::OnFullscreenStateChanged() {
  voice_plate_->SetRenderOverWebContents(
      ShouldRenderOverWebContents(anchored_browser_));
}

}  // namespace ttc
