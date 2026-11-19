// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_OVERLAY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

class BrowserWindowInterface;
class Profile;

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class View;
class Widget;
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

  // Briefly marks `points` on the page, so that the user sees where the
  // get_pointer tool took them to be pointing. They are in DIPs relative to
  // the top left of the active tab's contents in the window the views are in.
  void ShowPointerMarks(base::span<const gfx::Point> points);

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  const std::vector<std::unique_ptr<views::Widget>>& pointer_marks_for_testing()
      const {
    return pointer_marks_;
  }

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

  // The marks of the latest ShowPointerMarks() call, in `anchored_browser_`.
  // They may have faded out.
  std::vector<std::unique_ptr<views::Widget>> pointer_marks_;

  // Fullscreen changes of `anchored_browser_`.
  base::CallbackListSubscription fullscreen_subscription_;

  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_OVERLAY_CONTROLLER_H_
