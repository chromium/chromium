// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/overlay_controller.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/ttc/ttc_view.h"
#include "chrome/browser/ui/views/ttc/ttc_view_delegate.h"
#include "chrome/browser/ui/views/ttc/ttc_voice_plate.h"
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
  if (!browser_shutdown::HasShutdownStarted()) {
    MoveToWindow(
        browser_collection_observation_.GetSource()->FindTabbedBrowser());
  }
  if (anchored_browser_ != browser) {
    return;
  }

  // The views are recreated when a tabbed window is next activated.
  anchored_browser_ = nullptr;
  views_.clear();
}

void OverlayController::MoveToWindow(BrowserWindowInterface* browser) {
  views::View* anchor_view = GetAnchorView(browser);
  if (!anchor_view) {
    return;
  }

  if (browser == anchored_browser_) {
    return;
  }

  anchored_browser_ = browser;

  // Re-parenting the existing views turned out to be somewhat fraught, at least
  // on Linux, so we recreate the views on each window move.
  views_.clear();
  CreateViews(anchor_view);
}

void OverlayController::CreateViews(views::View* anchor_view) {
  CHECK(views_.empty());
  // The views are recreated on every window move, so they can't keep state of
  // their own.
  views_.push_back(std::make_unique<TtcVoicePlate>(anchor_view, *delegate_));

  for (const std::unique_ptr<TtcView>& view : views_) {
    view->ApplyUiState(ui_state_);
    view->Show();
  }
}

}  // namespace ttc
