// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_H_
#define ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_H_

namespace ash {

// Indicator for the locations that the pod is able to be placed in. We only
// support two for the sake of simplicity for now.
enum class OnTaskPodSnapLocation { kTopLeft, kTopRight };

// Controller interface used by the `OnTaskPodView` to supplement OnTask UX with
// convenience features like page navigation, tab reloads, etc.
class OnTaskPodController {
 public:
  OnTaskPodController(const OnTaskPodController&) = delete;
  OnTaskPodController& operator=(const OnTaskPodController&) = delete;
  virtual ~OnTaskPodController() = default;

  // Configures the snap location for the current pod instance.
  virtual void SetSnapLocation(OnTaskPodSnapLocation snap_location) = 0;

  // Attempts to navigate back to the previous page.
  virtual void MaybeNavigateToPreviousPage() = 0;

  // Attempts to navigate forward to the next page.
  virtual void MaybeNavigateToNextPage() = 0;

  // Attempts to reload the current page.
  virtual void ReloadCurrentPage() = 0;

  // Attempts to show or hide the tab strip.
  virtual void ToggleTabStripVisibility(bool show, bool user_action) = 0;

  // Notifies pod widget when the app is paused or unpaused.
  virtual void OnPauseModeChanged(bool paused) = 0;

  // Notifies pod widget when there is an update in the page navigation context
  // (tab switch, URL navigation, etc.).
  virtual void OnPageNavigationContextChanged() = 0;

  // Whether the boca browser can navigate to the previous page.
  virtual bool CanNavigateToPreviousPage() = 0;

  // Whether the boca browser can navigate to the next page.
  virtual bool CanNavigateToNextPage() = 0;

  // Whether the show or hide tab button should be enabled or not.
  virtual bool CanToggleTabStripVisibility() = 0;

 protected:
  OnTaskPodController() = default;
};

}  // namespace ash

#endif  // ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_H_
