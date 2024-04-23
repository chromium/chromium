// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_session_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "ui/base/ime/text_input_client.h"

namespace ash {
namespace {

namespace ms = metrics::structured;
namespace cros_events = metrics::structured::events::v2::cr_os_events;

cros_events::PickerInputFieldType GetInputFieldType(
    ui::TextInputClient* client) {
  if (client == nullptr) {
    return cros_events::PickerInputFieldType::NONE;
  }
  switch (client->GetTextInputType()) {
    case ui::TEXT_INPUT_TYPE_NONE:
      return cros_events::PickerInputFieldType::NONE;
    case ui::TEXT_INPUT_TYPE_TEXT:
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
      if (client->CanInsertImage()) {
        return cros_events::PickerInputFieldType::RICH_TEXT;
      } else {
        return cros_events::PickerInputFieldType::PLAIN_TEXT;
      }
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return cros_events::PickerInputFieldType::PASSWORD;
    case ui::TEXT_INPUT_TYPE_SEARCH:
      return cros_events::PickerInputFieldType::SEARCH;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return cros_events::PickerInputFieldType::EMAIL;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return cros_events::PickerInputFieldType::NUMBER;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return cros_events::PickerInputFieldType::TELEPHONE;
    case ui::TEXT_INPUT_TYPE_URL:
      return cros_events::PickerInputFieldType::URL;
    case ui::TEXT_INPUT_TYPE_DATE:
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
    case ui::TEXT_INPUT_TYPE_MONTH:
    case ui::TEXT_INPUT_TYPE_TIME:
    case ui::TEXT_INPUT_TYPE_WEEK:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return cros_events::PickerInputFieldType::DATE_TIME;
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
    case ui::TEXT_INPUT_TYPE_NULL:
      return cros_events::PickerInputFieldType::OTHER;
  }
}

int GetSelectionLength(ui::TextInputClient* client) {
  if (!client) {
    return 0;
  }
  gfx::Range selection_range;
  client->GetEditableSelectionRange(&selection_range);
  if (selection_range.IsValid() && !selection_range.is_empty()) {
    return selection_range.length();
  }
  return 0;
}

}  // namespace

PickerSessionMetrics::PickerSessionMetrics() = default;

PickerSessionMetrics::~PickerSessionMetrics() {
  if (recorded_outcome_) {
    return;
  }

  RecordOutcome(SessionOutcome::kUnknown);
}

void PickerSessionMetrics::RecordOutcome(SessionOutcome outcome) {
  if (recorded_outcome_) {
    return;
  }

  base::UmaHistogramEnumeration("Ash.Picker.Session.Outcome", outcome);
  recorded_outcome_ = true;
}

void PickerSessionMetrics::OnStartSession(ui::TextInputClient* client) {
  ms::StructuredMetricsClient::Record(
      cros_events::Picker_StartSession()
          .SetInputFieldType(GetInputFieldType(client))
          .SetSelectionLength(GetSelectionLength(client)));
}

}  // namespace ash
