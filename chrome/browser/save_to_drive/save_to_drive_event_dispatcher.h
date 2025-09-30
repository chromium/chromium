// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_EVENT_DISPATCHER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

namespace extensions::api::pdf_viewer_private {
struct SaveToDriveProgress;
}  // namespace extensions::api::pdf_viewer_private

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace save_to_drive {
class SaveToDriveRecorder;
class TimeRemainingCalculator;

// This class is used to dispatch events to the PDF viewer extension from the
// browser process. The owner of this object must ensure that the browser
// context is valid for the lifetime of this object.
class SaveToDriveEventDispatcher {
 public:
  static std::unique_ptr<SaveToDriveEventDispatcher> Create(
      content::RenderFrameHost* render_frame_host);

  static std::unique_ptr<SaveToDriveEventDispatcher> CreateForTesting(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<TimeRemainingCalculator> time_remaining_calculator,
      std::unique_ptr<SaveToDriveRecorder> recorder);

  virtual ~SaveToDriveEventDispatcher();

  // Dispatches a save to drive progress event to the PDF viewer extension.
  // `progress` must have a status and error type that is not none. The function
  // is marked virtual for testing purposes.
  virtual void Notify(
      extensions::api::pdf_viewer_private::SaveToDriveProgress progress) const;

 protected:
  SaveToDriveEventDispatcher(
      content::RenderFrameHost* render_frame_host,
      const GURL& stream_url,
      std::unique_ptr<TimeRemainingCalculator> time_remaining_calculator,
      std::unique_ptr<SaveToDriveRecorder> recorder);

 private:
  // Returns a string for the current upload state.
  // Example: "100/120 MB • 10 seconds left", "100 MB • Done"
  std::optional<std::string> GetFileMetadataString(
      const extensions::api::pdf_viewer_private::SaveToDriveProgress& progress)
      const;

  const raw_ptr<content::BrowserContext> browser_context_;
  // The stream URL of the PDF that is being saved to Drive.
  const GURL stream_url_;
  // The calculator used to calculate the time remaining for upload.
  std::unique_ptr<TimeRemainingCalculator> time_remaining_calculator_;
  // The recorder for Save to Drive metrics.
  std::unique_ptr<SaveToDriveRecorder> recorder_;
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_EVENT_DISPATCHER_H_
