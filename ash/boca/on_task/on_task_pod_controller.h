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

  // Attempts to reload the current page.
  virtual void ReloadCurrentPage() = 0;

 protected:
  OnTaskPodController() = default;
};

}  // namespace ash

#endif  // ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_H_
