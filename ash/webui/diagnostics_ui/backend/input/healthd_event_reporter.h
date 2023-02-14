// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_HEALTHD_EVENT_REPORTER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_HEALTHD_EVENT_REPORTER_H_

#include "ash/system/diagnostics/mojom/input.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_event_reporters.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::diagnostics {

// A helper class for reporting keyboard diagnostic events to HealthD service.
class HealthdEventReporter {
 public:
  HealthdEventReporter();
  HealthdEventReporter(const HealthdEventReporter&) = delete;
  HealthdEventReporter& operator=(const HealthdEventReporter&) = delete;
  ~HealthdEventReporter();

  // Adds a key event of a keyboard. This event will be contained in the next
  // diagnostic event of that keyboard.
  void AddKeyEventForNextReport(uint32_t keyboard_id,
                                const mojom::KeyEventPtr& key_event);

  // Reports the keyboard diagnostic event.
  void ReportKeyboardDiagnosticEvent(
      uint32_t keyboard_id,
      const mojom::KeyboardInfoPtr& keyboard_info);

 private:
  // Mojo interface to report the event to HealthD.
  mojo::Remote<::ash::cros_healthd::mojom::AshEventReporter>
      healthd_ash_event_reporter_;

  // These keep the keycode / top row key until the event being sent.
  std::map<uint32_t, std::set<uint32_t>> tested_keycode_;
  std::map<uint32_t, std::set<uint32_t>> tested_top_row_key_;
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_HEALTHD_EVENT_REPORTER_H_
