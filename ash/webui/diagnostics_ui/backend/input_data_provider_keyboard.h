// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_KEYBOARD_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_KEYBOARD_H_

#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "base/memory/weak_ptr.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"

namespace ash {
namespace diagnostics {

class InputDeviceInformation;

// Helper to provide InputDataProvider diagnostic interface with
// keyboard-specific logic.
class InputDataProviderKeyboard {
 public:
  InputDataProviderKeyboard();
  InputDataProviderKeyboard(const InputDataProviderKeyboard&) = delete;
  InputDataProviderKeyboard& operator=(const InputDataProviderKeyboard&) =
      delete;
  ~InputDataProviderKeyboard();

  void GetKeyboardVisualLayout(
      mojom::KeyboardInfoPtr keyboard,
      mojom::InputDataProvider::GetKeyboardVisualLayoutCallback callback);

  mojom::KeyboardInfoPtr ConstructKeyboard(
      const InputDeviceInformation* device_info);

 private:
  void ProcessXkbLayout(
      mojom::InputDataProvider::GetKeyboardVisualLayoutCallback callback);
  mojom::KeyGlyphSetPtr LookupGlyphSet(uint32_t evdev_code);

  void ProcessKeyboardTopRowLayout(
      const InputDeviceInformation* device_info,
      ui::EventRewriterChromeOS::KeyboardTopRowLayout* out_top_row_layout,
      std::vector<mojom::TopRowKey>* out_top_row_keys);

  ui::XkbEvdevCodes xkb_evdev_codes_;
  ui::XkbKeyboardLayoutEngine xkb_layout_engine_;

  base::WeakPtrFactory<InputDataProviderKeyboard> weak_factory_{this};
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_KEYBOARD_H_
