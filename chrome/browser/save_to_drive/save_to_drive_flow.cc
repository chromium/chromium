// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_flow.h"

#include "chrome/browser/save_to_drive/save_to_drive_event_dispatcher.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"

namespace save_to_drive {

SaveToDriveFlow::SaveToDriveFlow(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher)
    : content::DocumentUserData<SaveToDriveFlow>(render_frame_host),
      event_dispatcher_(std::move(event_dispatcher)) {}

SaveToDriveFlow::~SaveToDriveFlow() = default;

void SaveToDriveFlow::Run() {
  // TODO(crbug.com/424208776): Implement the flow.
}

void SaveToDriveFlow::Stop() {
  DeleteForCurrentDocument(&render_frame_host());
  // Don't do anything else here. The flow will be destroyed after this line.
}

DOCUMENT_USER_DATA_KEY_IMPL(SaveToDriveFlow);

}  // namespace save_to_drive
