// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_

#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/native_widget_types.h"

// This bridge is responsible for triggering all the variants of the access loss
// warning sheet from the cpp side.
class PasswordAccessLossWarningBridge {
 public:
  virtual ~PasswordAccessLossWarningBridge() = default;

  // Determines if any of the access loss warning sheets should be shown.
  virtual bool ShouldShowAccessLossNoticeSheet(PrefService* pref_service,
                                               bool called_at_startup) = 0;
  // Tries to call the Java code that will show an access loss warning sheet.
  // Showing the sheet can fail if there is no BottomSheetController or the
  // BottomSheetcontroller suppresses the sheet. Content is suppressed if higher
  // priority content is in the sheet, the sheet is expanded beyond the peeking
  // state, or the browser is in a mode that does not support showing the sheet.
  virtual void MaybeShowAccessLossNoticeSheet(
      PrefService* pref_service,
      const gfx::NativeWindow window,
      Profile* profile,
      bool called_at_startup,
      password_manager_android_util::PasswordAccessLossWarningTriggers
          trigger_source) = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_
