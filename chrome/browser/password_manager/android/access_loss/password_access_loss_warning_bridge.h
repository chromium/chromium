// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_

#include "components/prefs/pref_service.h"
#include "ui/gfx/native_widget_types.h"

// This bridge is responsible for triggering all the variants of the access loss
// warning sheet from the cpp side.
class PasswordAccessLossWarningBridge {
 public:
  virtual ~PasswordAccessLossWarningBridge() = default;

  // Determines if any of the access loss warning sheets should be shown.
  virtual bool ShouldShowAccessLossNoticeSheet(PrefService* pref_service) = 0;
  // Tries to call the Java code that will show an access loss warning sheet.
  // Showing the sheet can fail if there is no BottomSheetController or the
  // BottomSheetcontroller suppresses the sheet. Content is suppressed if higher
  // priority content is in the sheet, the sheet is expanded beyond the peeking
  // state, or the browser is in a mode that does not support showing the sheet.
  virtual void MaybeShowAccessLossNoticeSheet(
      PrefService* pref_service,
      const gfx::NativeWindow window) = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_
