// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_action_on_next_focus_request.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"

namespace ash {

PickerActionOnNextFocusRequest::PickerActionOnNextFocusRequest(
    ui::InputMethod* input_method,
    base::TimeDelta action_timeout,
    base::OnceClosure action_callback,
    base::OnceClosure timeout_callback)
    : action_callback_(std::move(action_callback)),
      timeout_callback_(std::move(timeout_callback)) {
  observation_.Observe(input_method);
  action_timeout_timer_.Start(FROM_HERE, action_timeout, this,
                              &PickerActionOnNextFocusRequest::OnTimeout);
}

PickerActionOnNextFocusRequest::~PickerActionOnNextFocusRequest() = default;

void PickerActionOnNextFocusRequest::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  ui::TextInputClient* mutable_client =
      observation_.GetSource()->GetTextInputClient();
  CHECK_EQ(mutable_client, client);
  if (mutable_client == nullptr ||
      mutable_client->GetTextInputType() ==
          ui::TextInputType::TEXT_INPUT_TYPE_NONE) {
    return;
  }

  action_timeout_timer_.Stop();
  observation_.Reset();
  std::move(action_callback_).Run();
}

void PickerActionOnNextFocusRequest::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  if (observation_.GetSource() == input_method) {
    observation_.Reset();
  }
}

void PickerActionOnNextFocusRequest::OnTimeout() {
  observation_.Reset();
  std::move(timeout_callback_).Run();
}

}  // namespace ash
