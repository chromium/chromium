// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_H_
#define ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_H_

namespace ash {

// Controller interface used by the `OnTaskPodView` to supplement OnTask UX with
// convenience features like page navigation, tab reloads, etc.
class OnTaskPodController {
 public:
  OnTaskPodController(const OnTaskPodController&) = delete;
  OnTaskPodController& operator=(const OnTaskPodController&) = delete;
  virtual ~OnTaskPodController() = default;

  // Attempts to reload the current page.
  virtual void ReloadCurrentPage() = 0;

 protected:
  OnTaskPodController() = default;
};

}  // namespace ash

#endif  // ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_H_
