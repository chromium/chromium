// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SET_TIME_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_SET_TIME_DIALOG_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

// Set Time dialog for setting the system time, date and time zone.
class SetTimeDialog : public SystemWebDialogDelegate {
 public:
  // Shows the set time/date dialog. If |parent| is not null, shows the dialog
  // as a child of |parent|, e.g. the Settings window.
  static void ShowDialog(gfx::NativeWindow parent = nullptr);

  // Returns true if the dialog should show the timezone <select>.
  static bool ShouldShowTimezone();

 private:
  SetTimeDialog();
  ~SetTimeDialog() override;

  // SystemWebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;

  DISALLOW_COPY_AND_ASSIGN(SetTimeDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SET_TIME_DIALOG_H_
