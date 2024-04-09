// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/screen_capture_notification_ui_ash.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace ash {

ScreenCaptureNotificationUIAsh::ScreenCaptureNotificationUIAsh(
    const std::u16string& text)
    : text_(text) {}

ScreenCaptureNotificationUIAsh::~ScreenCaptureNotificationUIAsh() {
  // MediaStreamCaptureIndicator will delete ScreenCaptureNotificationUI object
  // after it stops screen capture.
  stop_callback_.Reset();
  ash::Shell::Get()->system_tray_notifier()->NotifyScreenAccessStop();
}

gfx::NativeViewId ScreenCaptureNotificationUIAsh::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback,
    const std::vector<content::DesktopMediaID>& media_ids) {
  stop_callback_ = std::move(stop_callback);
  ash::Shell::Get()->system_tray_notifier()->NotifyScreenAccessStart(
      base::BindRepeating(
          &ScreenCaptureNotificationUIAsh::ProcessStopRequestFromUI,
          weak_ptr_factory_.GetWeakPtr()),
      source_callback
          ? base::BindRepeating(std::move(source_callback),
                                content::DesktopMediaID(),
                                /*captured_surface_control_active=*/false)
          : base::RepeatingClosure(),
      text_);
  return 0;
}

void ScreenCaptureNotificationUIAsh::ProcessStopRequestFromUI() {
  if (!stop_callback_.is_null()) {
    std::move(stop_callback_).Run();
  }
}

}  // namespace ash

// static
std::unique_ptr<ScreenCaptureNotificationUI>
ScreenCaptureNotificationUI::Create(
    const std::u16string& text,
    content::WebContents* capturing_web_contents) {
  return std::make_unique<ash::ScreenCaptureNotificationUIAsh>(text);
}
