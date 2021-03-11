// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {

// Shows a modal warning dialog that the idle action is imminent. Since the
// warning is only really necessary when the idle action is to log out the user,
// the warning is hard-coded to warn about logout.
class IdleActionWarningDialogView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(IdleActionWarningDialogView);
  explicit IdleActionWarningDialogView(base::TimeTicks idle_action_time);
  IdleActionWarningDialogView(const IdleActionWarningDialogView&) = delete;
  IdleActionWarningDialogView& operator=(const IdleActionWarningDialogView&) =
      delete;

  void CloseDialog();

  void Update(base::TimeTicks idle_action_time);

  // views::DialogDelegateView:
  std::u16string GetWindowTitle() const override;

 private:
  ~IdleActionWarningDialogView() override;

  void UpdateTitle();

  base::TimeTicks idle_action_time_;
  base::RepeatingTimer update_timer_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_
