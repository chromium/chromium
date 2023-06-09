// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_

#include "ash/webui/file_manager/file_manager_ui_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebUI;
}  // namespace content

// Chrome browser FileManagerUIDelegate implementation.
class ChromeFileManagerUIDelegate : public ash::FileManagerUIDelegate {
 public:
  explicit ChromeFileManagerUIDelegate(content::WebUI* web_ui);
  ~ChromeFileManagerUIDelegate() override;

  ChromeFileManagerUIDelegate(const ChromeFileManagerUIDelegate&) = delete;
  ChromeFileManagerUIDelegate& operator=(const ChromeFileManagerUIDelegate&) =
      delete;

  // Fetches a map that maps message IDs to actual strings shown to the user.
  // Extends the map with properties used by the files app, such as which
  // features are enabled. Returns the populated map to the caller.
  base::Value::Dict GetLoadTimeData() const override;

  // Calls volume manager io_task_controller ProgressPausedTasks API to make
  // I/O state::PAUSED tasks emit their IOTask progress status.
  void ProgressPausedTasks() const override;

  // Whether to turn on or off the polling of hosted documents pin and available
  // offline states. If already enabled, this will not do anything.
  void ShouldPollDriveHostedPinStates(bool enabled) override;

 private:
  void PollHostedPinStates();

  // When true, repeatedly poll the DriveFS hosted documents pin states.
  bool poll_hosted_pin_states_ = false;

  raw_ptr<content::WebUI, ExperimentalAsh> web_ui_;  // Owns |this|.

  base::WeakPtrFactory<ChromeFileManagerUIDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_
