// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PIN_STATUS_MESSAGE_VIEW_H_
#define ASH_LOGIN_UI_PIN_STATUS_MESSAGE_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

// The message that can be shown to the user when the PIN is soft-locked.
class PinStatusMessageView : public views::View {
 public:
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(PinStatusMessageView* view);
    ~TestApi();

    const std::u16string& GetPinStatusMessageContent() const;

   private:
    const raw_ptr<PinStatusMessageView> view_;
  };

  using OnPinUnlock = base::RepeatingClosure;

  explicit PinStatusMessageView(base::RepeatingClosure on_pin_unlocked);

  PinStatusMessageView(const PinStatusMessageView&) = delete;
  PinStatusMessageView& operator=(const PinStatusMessageView&) = delete;

  ~PinStatusMessageView() override;

  // Set the relevant PIN information (pin available time, if pin the only
  // auth factor) to be shown in the message.
  void SetPinInfo(base::Time available_at, bool is_pin_only);

  // views::View:
  void RequestFocus() override;

 private:
  // Refresh the UI to show the latest remaining time.
  void UpdateUI();

  raw_ptr<views::Label> message_;

  OnPinUnlock on_pin_unlock_;
  bool is_pin_only_;
  base::Time available_at_;
  base::MetronomeTimer timer_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PIN_STATUS_MESSAGE_VIEW_H_
