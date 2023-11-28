// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/healthd_event_reporter.h"

#include "base/time/time.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash::diagnostics {
namespace {

// A timeout to prevent the initialization waiting forever. Usually not going to
// be reached.
constexpr base::TimeDelta kReporterInitializationTimeout = base::Minutes(2);

std::vector<uint32_t> ExtractSetToVector(
    uint32_t key,
    std::map<uint32_t, std::set<uint32_t>>& map) {
  auto node = map.extract(key);
  if (!node) {
    return {};
  }
  return {node.mapped().begin(), node.mapped().end()};
}

}  // namespace

HealthdEventReporter::HealthdEventReporter() {
  mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosHealthdAshEventReporter,
      std::make_optional<base::TimeDelta>(kReporterInitializationTimeout),
      healthd_ash_event_reporter_.BindNewPipeAndPassReceiver().PassPipe());
}

HealthdEventReporter::~HealthdEventReporter() = default;

void HealthdEventReporter::AddKeyEventForNextReport(
    uint32_t keyboard_id,
    const mojom::KeyEventPtr& key_event) {
  if (key_event->top_row_position != -1) {
    tested_top_row_key_[keyboard_id].insert(key_event->top_row_position);
  } else {
    tested_keycode_[keyboard_id].insert(key_event->key_code);
  }
}

void HealthdEventReporter::ReportKeyboardDiagnosticEvent(
    uint32_t keyboard_id,
    const mojom::KeyboardInfoPtr& keyboard_info) {
  auto event = mojom::KeyboardDiagnosticEventInfo::New();
  event->keyboard_info = keyboard_info.Clone();
  event->tested_keys = ExtractSetToVector(keyboard_id, tested_keycode_);
  event->tested_top_row_keys =
      ExtractSetToVector(keyboard_id, tested_top_row_key_);
  healthd_ash_event_reporter_->SendKeyboardDiagnosticEvent(std::move(event));
}

}  // namespace ash::diagnostics
