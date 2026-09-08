// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DATA_PROTECTION_TAB_CAPTURE_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DATA_PROTECTION_TAB_CAPTURE_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"

static_assert(BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION),
              "DataProtectionTabCaptureHandler requires "
              "ENTERPRISE_SCREENSHOT_PROTECTION");

namespace content {
class WebContents;
}  // namespace content

// Handles Enterprise Data Protection screenshot and tab-sharing policy updates
// from DataProtectionNavigationController for the WebRTC MediaStream pipeline.
// Pauses the stream when screenshots/sharing are disallowed and resumes when
// allowed.
class DataProtectionTabCaptureHandler {
 public:
  DataProtectionTabCaptureHandler(
      content::WebContents* captured_contents,
      const content::DesktopMediaID& media_id,
      content::MediaStreamUI::StateChangeCallback state_change_callback);
  DataProtectionTabCaptureHandler(const DataProtectionTabCaptureHandler&) =
      delete;
  DataProtectionTabCaptureHandler& operator=(
      const DataProtectionTabCaptureHandler&) = delete;
  ~DataProtectionTabCaptureHandler();

  const content::DesktopMediaID& media_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return media_id_;
  }

 private:
  void OnScreenshotAllowedUpdated(bool allowed);

  const content::DesktopMediaID media_id_;
  content::MediaStreamUI::StateChangeCallback state_change_callback_;
  bool is_paused_ = false;

  base::CallbackListSubscription screenshot_allowed_subscription_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DataProtectionTabCaptureHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DATA_PROTECTION_TAB_CAPTURE_HANDLER_H_
