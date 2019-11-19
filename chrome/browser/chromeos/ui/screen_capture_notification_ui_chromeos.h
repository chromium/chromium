// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_SCREEN_CAPTURE_NOTIFICATION_UI_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_UI_SCREEN_CAPTURE_NOTIFICATION_UI_CHROMEOS_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"

namespace chromeos {

// Chromeos implementation for ScreenCaptureNotificationUI.
class ScreenCaptureNotificationUIChromeOS : public ScreenCaptureNotificationUI {
 public:
  // |text| is used to specify the text for the notification.
  explicit ScreenCaptureNotificationUIChromeOS(const base::string16& text);
  ~ScreenCaptureNotificationUIChromeOS() override;

  // ScreenCaptureNotificationUI overrides.
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback) override;

 private:
  void ProcessStopRequestFromUI();

  const base::string16 text_;
  base::OnceClosure stop_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUIChromeOS);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_UI_SCREEN_CAPTURE_NOTIFICATION_UI_CHROMEOS_H_
