// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_insertion.h"

#include "ash/lobster/lobster_image_actuator.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"

namespace {

constexpr base::TimeDelta kInsertionTimeout = base::Seconds(1);

ui::TextInputClient* GetFocusedTextInputClient() {
  if (ash::IMEBridge::Get() == nullptr ||
      ash::IMEBridge::Get()->GetInputContextHandler() == nullptr) {
    return nullptr;
  }

  const ui::InputMethod* input_method =
      ash::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method || !input_method->GetTextInputClient()) {
    return nullptr;
  }

  return input_method->GetTextInputClient();
}

}  // namespace

LobsterInsertion::LobsterInsertion(const std::string& image_bytes,
                                   ash::StatusCallback insert_status_callback)
    : pending_image_bytes_(image_bytes),
      state_(State::kPending),
      insert_status_callback_(std::move(insert_status_callback)) {
  insertion_timeout_.Start(FROM_HERE, kInsertionTimeout,
                           base::BindOnce(&LobsterInsertion::CancelInsertion,
                                          weak_ptr_factory_.GetWeakPtr()));
}

LobsterInsertion::~LobsterInsertion() = default;

bool LobsterInsertion::HasTimedOut() {
  return state_ == State::kTimedOut;
}

bool LobsterInsertion::Commit() {
  ui::TextInputClient* input_client = GetFocusedTextInputClient();

  if (HasTimedOut() || input_client == nullptr) {
    std::move(insert_status_callback_).Run(false);
    return false;
  }
  insertion_timeout_.Stop();

  bool success =
      ash::InsertImageOrCopyToClipboard(input_client, pending_image_bytes_);
  std::move(insert_status_callback_).Run(success);
  return success;
}

void LobsterInsertion::CancelInsertion() {
  state_ = State::kTimedOut;
}
