// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_
#define CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// Shows a modal warning dialog that the idle action is imminent. Since the
// warning is only really necessary when the idle action is to log out the user,
// the warning is hard-coded to warn about logout.
class IdleActionWarningDialogView : public views::DialogDelegateView {
  METADATA_HEADER(IdleActionWarningDialogView, views::DialogDelegateView)

 public:
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

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_DIALOG_VIEW_H_
