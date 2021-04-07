// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_SCREEN_CAPTURE_NOTIFICATION_UI_ASH_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_SCREEN_CAPTURE_NOTIFICATION_UI_ASH_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"

namespace ash {

// Ash implementation for ScreenCaptureNotificationUI.
class ScreenCaptureNotificationUIAsh : public ScreenCaptureNotificationUI {
 public:
  // |text| is used to specify the text for the notification.
  explicit ScreenCaptureNotificationUIAsh(const std::u16string& text);
  ~ScreenCaptureNotificationUIAsh() override;

  // ScreenCaptureNotificationUI overrides.
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback) override;

 private:
  void ProcessStopRequestFromUI();

  const std::u16string text_;
  base::OnceClosure stop_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUIAsh);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_SCREEN_CAPTURE_NOTIFICATION_UI_ASH_H_
