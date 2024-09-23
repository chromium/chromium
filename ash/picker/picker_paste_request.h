// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_PASTE_REQUEST_H_
#define ASH_PICKER_PICKER_PASTE_REQUEST_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class ClipboardHistoryController;

// Pastes a clipboard item on the next window focus change.
class ASH_EXPORT PickerPasteRequest : public aura::client::FocusChangeObserver {
 public:
  // Creates a request to paste `clipboard_item_id` in the next focused Widget.
  explicit PickerPasteRequest(
      ClipboardHistoryController* clipboard_history_controller,
      aura::client::FocusClient* focus_client,
      base::UnguessableToken clipboard_item_id);
  ~PickerPasteRequest() override;

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

 private:
  raw_ref<ClipboardHistoryController> clipboard_history_controller_;
  base::UnguessableToken clipboard_item_id_;
  base::ScopedObservation<aura::client::FocusClient,
                          aura::client::FocusChangeObserver>
      observation_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_PASTE_REQUEST_H_
