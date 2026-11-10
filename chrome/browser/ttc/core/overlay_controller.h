// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_OVERLAY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

class BrowserWindowInterface;
class Profile;

namespace views {
class View;
}  // namespace views

namespace ttc {

class TtcView;
class TtcViewDelegate;
class TtcVoicePlate;

// Owns the TTC views shown on top of the browser window and keeps them in the
// tabbed browser window the user is currently working in.
class OverlayController : public BrowserCollectionObserver {
 public:
  OverlayController(Profile& profile, TtcViewDelegate& delegate);
  ~OverlayController() override;
  OverlayController(const OverlayController&) = delete;
  OverlayController& operator=(const OverlayController&) = delete;

  void ApplyUiState(UiState ui_state);

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

 private:
  void MoveToWindow(BrowserWindowInterface* browser);

  void CreateViews(views::View* anchor_view);
  void OnFullscreenStateChanged();

  // Safe because the delegate is guaranteed to outlive this object.
  const raw_ref<TtcViewDelegate> delegate_;

  // The latest UiState applied, which views get when they're created.
  UiState ui_state_ = UiState::kInitializing;

  // The window the views are currently anchored in. Null iff `views_` is empty.
  raw_ptr<BrowserWindowInterface> anchored_browser_ = nullptr;

  // Empty if there was no suitable window to show the overlay in.
  std::vector<std::unique_ptr<TtcView>> views_;

  // Owned by `views_`; null iff `views_` is empty.
  raw_ptr<TtcVoicePlate> voice_plate_ = nullptr;

  // Fullscreen changes of `anchored_browser_`.
  base::CallbackListSubscription fullscreen_subscription_;

  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_OVERLAY_CONTROLLER_H_
