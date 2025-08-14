// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_FLOW_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_FLOW_H_

#include "content/public/browser/document_user_data.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace save_to_drive {

class SaveToDriveEventDispatcher;

// This class is responsible for orchastrating the entire save to Drive flow
// on the browser process from showing the account chooser, reading the file
// size, and initiating the appropriate drive uploader to save the file to
// Drive. This flow will be tied to the lifetime of the document. It is
// responsible for cleaning up its resources when the flow is stopped.
class SaveToDriveFlow : public content::DocumentUserData<SaveToDriveFlow> {
 public:
  SaveToDriveFlow(const SaveToDriveFlow&) = delete;
  SaveToDriveFlow& operator=(const SaveToDriveFlow&) = delete;
  ~SaveToDriveFlow() override;

  // Starts the save to Drive flow.
  void Run();

  // Cleans up the flow and its resources. This is called when the flow is
  // aborted or completed.
  void Stop();

 private:
  friend class content::DocumentUserData<SaveToDriveFlow>;

  SaveToDriveFlow(content::RenderFrameHost* render_frame_host,
                  std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher);

  std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher_;

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_FLOW_H_
