// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_HISTORY_IMPL_H_
#define ASH_ACCELERATORS_ACCELERATOR_HISTORY_IMPL_H_

#include <optional>
#include <set>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_handler.h"

namespace ash {

// Keeps track of the system-wide current and the last key accelerators.
class ASH_EXPORT AcceleratorHistoryImpl : public AcceleratorHistory,
                                          public ui::EventHandler {
 public:
  AcceleratorHistoryImpl();
  AcceleratorHistoryImpl(const AcceleratorHistoryImpl&) = delete;
  AcceleratorHistoryImpl& operator=(const AcceleratorHistoryImpl&) = delete;
  ~AcceleratorHistoryImpl() override;

  // Returns the most recent recorded accelerator.
  const ui::Accelerator& current_accelerator() const {
    return current_accelerator_;
  }

  // Returns the most recent previously recorded accelerator that is different
  // than the current.
  const ui::Accelerator& previous_accelerator() const {
    return previous_accelerator_;
  }

  const std::set<ui::KeyboardCode>& currently_pressed_keys() const {
    return currently_pressed_keys_;
  }

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // AcceleratorHistory:
  void StoreCurrentAccelerator(const ui::Accelerator& accelerator) override;

  void InterruptCurrentAccelerator();

 private:
  ui::Accelerator current_accelerator_;
  ui::Accelerator previous_accelerator_;

  std::set<ui::KeyboardCode> currently_pressed_keys_;

  // The most recently logged KeyboardCode, saved to prevent spammy logs.
  std::optional<ui::KeyboardCode> last_logged_key_code_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_HISTORY_IMPL_H_
