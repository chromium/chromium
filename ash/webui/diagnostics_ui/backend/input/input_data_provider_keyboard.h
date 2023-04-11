// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DATA_PROVIDER_KEYBOARD_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DATA_PROVIDER_KEYBOARD_H_

#include <vector>

#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/keyboard_capability.h"

namespace ash {
namespace diagnostics {

class InputDeviceInformation;

// Helper to provide InputDataProvider diagnostic interface with
// keyboard-specific logic.
class InputDataProviderKeyboard {
 public:
  // Holder for any data that needs to be persisted per keyboard, that
  // does not need to be exposed in the mojo::KeyboardInfo.
  class AuxData {
   public:
    AuxData();
    AuxData(const AuxData&) = delete;
    AuxData& operator=(const AuxData&) = delete;
    ~AuxData();

    // Map of scancodes that map to particular indexes within the top_row_keys
    // for that evdev. May contain AT and HID-style scancodes.
    base::flat_map<uint32_t, uint32_t> top_row_key_scancode_indexes;
  };

  InputDataProviderKeyboard();
  InputDataProviderKeyboard(const InputDataProviderKeyboard&) = delete;
  InputDataProviderKeyboard& operator=(const InputDataProviderKeyboard&) =
      delete;
  ~InputDataProviderKeyboard();

  mojom::KeyboardInfoPtr ConstructKeyboard(
      const InputDeviceInformation* device_info,
      AuxData* out_aux_data);

  mojom::KeyEventPtr ConstructInputKeyEvent(
      const mojom::KeyboardInfoPtr& keyboard,
      const AuxData* aux_data,
      uint32_t key_code,
      uint32_t scan_code,
      bool down);

 private:
  void ProcessKeyboardTopRowLayout(
      const InputDeviceInformation* device_info,
      ui::KeyboardCapability::KeyboardTopRowLayout top_row_layout,
      const std::vector<uint32_t>& scan_code_map,
      std::vector<mojom::TopRowKey>* out_top_row_keys,
      AuxData* out_aux_data);
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DATA_PROVIDER_KEYBOARD_H_
