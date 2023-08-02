// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_
#define ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_

#include "base/values.h"

namespace ash {

// Delegate to expose //chrome services to //components FileManagerUI.
class FileManagerUIDelegate {
 public:
  virtual ~FileManagerUIDelegate() = default;

  // Populates (writes) load time data to the source.
  virtual base::Value::Dict GetLoadTimeData() const = 0;

  // Calls volume manager io_task_controller ProgressPausedTasks API to make
  // I/O state::PAUSED tasks emit their IOTask progress status.
  virtual void ProgressPausedTasks() const = 0;

  // Toggle on or off the centralised polling of hosted document pin states.
  virtual void ShouldPollDriveHostedPinStates(bool enabled) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_
