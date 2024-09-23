// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CHANGE_DIALOG_H_
#define ASH_DISPLAY_DISPLAY_CHANGE_DIALOG_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// Modal system dialog that is displayed when a user changes the configuration
// of an external display.
class ASH_EXPORT DisplayChangeDialog : public views::DialogDelegateView {
 public:
  using CancelCallback = base::OnceCallback<void(bool display_was_removed)>;

  DisplayChangeDialog(std::u16string window_title,
                      std::u16string timeout_message_with_placeholder,
                      base::OnceClosure on_accept_callback,
                      CancelCallback on_cancel_callback);
  ~DisplayChangeDialog() override;

  DisplayChangeDialog(const DisplayChangeDialog&) = delete;
  DisplayChangeDialog& operator=(const DisplayChangeDialog&) = delete;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  base::WeakPtr<DisplayChangeDialog> GetWeakPtr();

 private:
  friend class ResolutionNotificationControllerTest;
  FRIEND_TEST_ALL_PREFIXES(ResolutionNotificationControllerTest, Timeout);

  static constexpr uint16_t kDefaultTimeoutInSeconds = 15;

  void OnConfirmButtonClicked();

  void OnCancelButtonClicked();

  void OnTimerTick();

  // Returns the string displayed as a message in the dialog which includes a
  // countdown timer.
  std::u16string GetRevertTimeoutString() const;

  // The remaining timeout in seconds.
  uint16_t timeout_count_ = kDefaultTimeoutInSeconds;

  const std::u16string timeout_message_with_placeholder_;

  raw_ptr<views::Label> label_ = nullptr;  // Not owned.
  base::OnceClosure on_accept_callback_;
  CancelCallback on_cancel_callback_;

  // The timer to invoke OnTimerTick() every second. This cannot be
  // OneShotTimer since the message contains text "automatically closed in xx
  // seconds..." which has to be updated every second.
  base::RepeatingTimer timer_;

  base::WeakPtrFactory<DisplayChangeDialog> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CHANGE_DIALOG_H_
