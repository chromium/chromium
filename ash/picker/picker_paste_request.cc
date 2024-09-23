// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_paste_request.h"

#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/check_deref.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/aura/client/focus_client.h"
#include "ui/events/event_constants.h"

namespace ash {

PickerPasteRequest::PickerPasteRequest(
    ClipboardHistoryController* clipboard_history_controller,
    aura::client::FocusClient* focus_client,
    base::UnguessableToken clipboard_item_id)
    : clipboard_history_controller_(CHECK_DEREF(clipboard_history_controller)),
      clipboard_item_id_(clipboard_item_id) {
  observation_.Observe(focus_client);
}

PickerPasteRequest::~PickerPasteRequest() = default;

void PickerPasteRequest::OnWindowFocused(aura::Window* gained_focus,
                                         aura::Window* lost_focus) {
  if (gained_focus == nullptr) {
    return;
  }

  // TODO: b/329309518: Use a dedicated show source for Picker.
  clipboard_history_controller_->PasteClipboardItemById(
      clipboard_item_id_.ToString(), ui::EF_NONE,
      crosapi::mojom::ClipboardHistoryControllerShowSource::kVirtualKeyboard);
  observation_.Reset();
}

}  // namespace ash
