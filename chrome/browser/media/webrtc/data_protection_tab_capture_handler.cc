// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/data_protection_tab_capture_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

DataProtectionTabCaptureHandler::DataProtectionTabCaptureHandler(
    content::WebContents* captured_contents,
    const content::DesktopMediaID& media_id,
    content::MediaStreamUI::StateChangeCallback state_change_callback)
    : media_id_(media_id),
      state_change_callback_(std::move(state_change_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enterprise_data_protection::DataProtectionNavigationController* controller =
      enterprise_data_protection::DataProtectionNavigationController::
          FromWebContents(captured_contents);
  if (!controller) {
    return;
  }
  screenshot_allowed_subscription_ =
      controller->RegisterScreenshotAllowedUpdatedCallback(base::BindRepeating(
          &DataProtectionTabCaptureHandler::OnScreenshotAllowedUpdated,
          weak_factory_.GetWeakPtr()));
  OnScreenshotAllowedUpdated(controller->screenshot_allowed());
}

DataProtectionTabCaptureHandler::~DataProtectionTabCaptureHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DataProtectionTabCaptureHandler::OnScreenshotAllowedUpdated(bool allowed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (allowed != is_paused_) {
    return;
  }
  is_paused_ = !allowed;
  if (state_change_callback_) {
    state_change_callback_.Run(
        media_id_, is_paused_ ? blink::mojom::MediaStreamStateChange::PAUSE
                              : blink::mojom::MediaStreamStateChange::PLAY);
  }
}
