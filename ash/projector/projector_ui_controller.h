// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
#define ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/projector/projector_metrics.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/raw_ptr.h"

namespace ash {

// The controller in charge of UI.
class ASH_EXPORT ProjectorUiController {
 public:
  // Shows a notification informing the user that a Projector error has
  // occurred.
  static void ShowFailureNotification(
      int message_id,
      int title_id = IDS_ASH_PROJECTOR_FAILURE_TITLE);

  // Shows a notification informing the user that a Projector save error has
  // occurred.
  static void ShowSaveFailureNotification();

  ProjectorUiController();
  ProjectorUiController(const ProjectorUiController&) = delete;
  ProjectorUiController& operator=(const ProjectorUiController&) = delete;
  ~ProjectorUiController();
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
