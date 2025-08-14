// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_EVENT_DISPATCHER_H_

#include <memory>

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

// This class is used to dispatch events to the PDF viewer extension from the
// browser process. The owner of this object must ensure that the browser
// context is valid for the lifetime of this object.
class SaveToDriveEventDispatcher {
 public:
  static std::unique_ptr<SaveToDriveEventDispatcher> Create(
      content::RenderFrameHost* render_frame_host);

  ~SaveToDriveEventDispatcher();

  // Dispatches a save to drive progress event to the PDF viewer extension.
  // `progress` must have a status and error type that is not none.
  void Notify(
      const extensions::api::pdf_viewer_private::SaveToDriveProgress& progress);

 private:
  SaveToDriveEventDispatcher(content::RenderFrameHost* render_frame_host,
                             const GURL& stream_url);

  const raw_ptr<content::BrowserContext> browser_context_;
  // The stream URL of the PDF that is being saved to Drive.
  const GURL stream_url_;
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_EVENT_DISPATCHER_H_
