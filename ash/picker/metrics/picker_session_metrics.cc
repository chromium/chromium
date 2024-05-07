// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_session_metrics.h"

#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "ui/base/ime/text_input_client.h"

namespace ash {
namespace {

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

cros_events::PickerSessionOutcome ConvertToCrosEventSessionOutcome(
    PickerSessionMetrics::SessionOutcome outcome) {
  switch (outcome) {
    case PickerSessionMetrics::SessionOutcome::kUnknown:
      return cros_events::PickerSessionOutcome::UNKNOWN;
    case PickerSessionMetrics::SessionOutcome::kInsertedOrCopied:
      return cros_events::PickerSessionOutcome::INSERTED_OR_COPIED;
    case PickerSessionMetrics::SessionOutcome::kAbandoned:
      return cros_events::PickerSessionOutcome::ABANDONED;
    case PickerSessionMetrics::SessionOutcome::kRedirected:
      return cros_events::PickerSessionOutcome::REDIRECTED;
    case PickerSessionMetrics::SessionOutcome::kFormat:
      return cros_events::PickerSessionOutcome::FORMAT;
  }
}

cros_events::PickerAction ConvertToCrosEventAction(
    std::optional<PickerCategory> action) {
  if (!action.has_value()) {
    return cros_events::PickerAction::UNKNOWN;
  }
  switch (*action) {
    case PickerCategory::kEditorWrite:
      return cros_events::PickerAction::OPEN_EDITOR_WRITE;
    case PickerCategory::kEditorRewrite:
      return cros_events::PickerAction::OPEN_EDITOR_REWRITE;
    case PickerCategory::kLinks:
      return cros_events::PickerAction::OPEN_LINKS;
    case PickerCategory::kExpressions:
      return cros_events::PickerAction::OPEN_EXPRESSIONS;
    case PickerCategory::kClipboard:
      return cros_events::PickerAction::OPEN_CLIPBOARD;
    case PickerCategory::kDriveFiles:
      return cros_events::PickerAction::OPEN_DRIVE_FILES;
    case PickerCategory::kLocalFiles:
      return cros_events::PickerAction::OPEN_LOCAL_FILES;
    case PickerCategory::kDatesTimes:
      return cros_events::PickerAction::OPEN_DATES_TIMES;
    case PickerCategory::kUnitsMaths:
      return cros_events::PickerAction::OPEN_UNITS_MATHS;
    case PickerCategory::kUpperCase:
      return cros_events::PickerAction::TRANSFORM_UPPER_CASE;
    case PickerCategory::kLowerCase:
      return cros_events::PickerAction::TRANSFORM_LOWER_CASE;
    case PickerCategory::kSentenceCase:
      return cros_events::PickerAction::TRANSFORM_SENTENCE_CASE;
    case PickerCategory::kTitleCase:
      return cros_events::PickerAction::TRANSFORM_TITLE_CASE;
    case PickerCategory::kCapsOn:
      return cros_events::PickerAction::CAPS_ON;
    case PickerCategory::kCapsOff:
      return cros_events::PickerAction::CAPS_OFF;
  }
}

cros_events::PickerResultSource GetResultSource(
    std::optional<PickerSearchResult> result) {
  if (!result.has_value()) {
    return cros_events::PickerResultSource::UNKNOWN;
  }
  using ReturnType = cros_events::PickerResultSource;
  return std::visit(
      base::Overloaded{
          [](const PickerSearchResult::TextData& data) {
            switch (data.source) {
              case PickerSearchResult::TextData::Source::kUnknown:
                return cros_events::PickerResultSource::UNKNOWN;
              case PickerSearchResult::TextData::Source::kDate:
                return cros_events::PickerResultSource::DATES_TIMES;
              case PickerSearchResult::TextData::Source::kMath:
                return cros_events::PickerResultSource::UNITS_MATHS;
              case PickerSearchResult::TextData::Source::kCaseTransform:
                return cros_events::PickerResultSource::CASE_TRANSFORM;
              case PickerSearchResult::TextData::Source::kOmnibox:
                return cros_events::PickerResultSource::OMNIBOX;
            }
          },
          [](const PickerSearchResult::EmojiData& data) {
            return cros_events::PickerResultSource::EMOJI;
          },
          [](const PickerSearchResult::SymbolData& data) {
            return cros_events::PickerResultSource::EMOJI;
          },
          [](const PickerSearchResult::EmoticonData& data) {
            return cros_events::PickerResultSource::EMOJI;
          },
          [](const PickerSearchResult::ClipboardData& data) {
            return cros_events::PickerResultSource::CLIPBOARD;
          },
          [](const PickerSearchResult::GifData& data) {
            return cros_events::PickerResultSource::TENOR;
          },
          [](const PickerSearchResult::BrowsingHistoryData& data) {
            return cros_events::PickerResultSource::OMNIBOX;
          },
          [](const PickerSearchResult::LocalFileData& data) {
            return cros_events::PickerResultSource::LOCAL_FILES;
          },
          [](const PickerSearchResult::DriveFileData& data) {
            return cros_events::PickerResultSource::DRIVE_FILES;
          },
          [](const PickerSearchResult::CategoryData& data) -> ReturnType {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::SearchRequestData& data) -> ReturnType {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::EditorData& data) -> ReturnType {
            NOTREACHED_NORETURN();
          },
      },
      result->data());
}

cros_events::PickerResultType GetResultType(
    std::optional<PickerSearchResult> result) {
  if (!result.has_value()) {
    return cros_events::PickerResultType::UNKNOWN;
  }
  using ReturnType = cros_events::PickerResultType;
  return std::visit(
      base::Overloaded{
          [](const PickerSearchResult::TextData& data) {
            return cros_events::PickerResultType::TEXT;
          },
          [](const PickerSearchResult::EmojiData& data) {
            return cros_events::PickerResultType::EMOJI;
          },
          [](const PickerSearchResult::SymbolData& data) {
            return cros_events::PickerResultType::SYMBOL;
          },
          [](const PickerSearchResult::EmoticonData& data) {
            return cros_events::PickerResultType::EMOTICON;
          },
          [](const PickerSearchResult::ClipboardData& data) {
            switch (data.display_format) {
              case PickerSearchResult::ClipboardData::DisplayFormat::kFile:
                return cros_events::PickerResultType::CLIPBOARD_FILE;
              case PickerSearchResult::ClipboardData::DisplayFormat::kText:
                return cros_events::PickerResultType::CLIPBOARD_TEXT;
              case PickerSearchResult::ClipboardData::DisplayFormat::kImage:
                return cros_events::PickerResultType::CLIPBOARD_IMAGE;
              case PickerSearchResult::ClipboardData::DisplayFormat::kHtml:
                return cros_events::PickerResultType::CLIPBOARD_HTML;
            }
          },
          [](const PickerSearchResult::GifData& data) {
            return cros_events::PickerResultType::GIF;
          },
          [](const PickerSearchResult::BrowsingHistoryData& data) {
            return cros_events::PickerResultType::LINK;
          },
          [](const PickerSearchResult::LocalFileData& data) {
            return cros_events::PickerResultType::LOCAL_FILE;
          },
          [](const PickerSearchResult::DriveFileData& data) {
            return cros_events::PickerResultType::DRIVE_FILE;
          },
          [](const PickerSearchResult::CategoryData& data) -> ReturnType {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::SearchRequestData& data) -> ReturnType {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::EditorData& data) -> ReturnType {
            NOTREACHED_NORETURN();
          },
      },
      result->data());
}

}  // namespace

PickerSessionMetrics::PickerSessionMetrics() = default;

PickerSessionMetrics::~PickerSessionMetrics() {
  OnFinishSession();
}

void PickerSessionMetrics::SetOutcome(SessionOutcome outcome) {
  if (outcome_ == SessionOutcome::kUnknown) {
    outcome_ = outcome;
  }
}

void PickerSessionMetrics::SetAction(PickerCategory action) {
  if (!action_.has_value()) {
    action_ = action;
  }
}

void PickerSessionMetrics::SetInsertedResult(PickerSearchResult inserted_result,
                                             int index) {
  if (!inserted_result_.has_value()) {
    inserted_result_ = std::move(inserted_result);
    result_index_ = index;
  }
}

void PickerSessionMetrics::UpdateSearchQuery(std::u16string_view search_query) {
  int new_length = static_cast<int>(search_query.length());
  search_query_total_edits_ += abs(new_length - search_query_length_);
  search_query_length_ = new_length;
}

void PickerSessionMetrics::OnStartSession(ui::TextInputClient* client) {
  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::Picker_StartSession()
                    .SetInputFieldType(GetInputFieldType(client))
                    .SetSelectionLength(
                        static_cast<int64_t>(GetSelectionLength(client)))));
}

void PickerSessionMetrics::OnFinishSession() {
  base::UmaHistogramEnumeration("Ash.Picker.Session.Outcome", outcome_);
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::Picker_FinishSession()
          .SetOutcome(ConvertToCrosEventSessionOutcome(outcome_))
          .SetAction(ConvertToCrosEventAction(action_))
          .SetResultSource(GetResultSource(std::move(inserted_result_)))
          .SetResultType(GetResultType(std::move(inserted_result_)))
          .SetTotalEdits(search_query_total_edits_)
          .SetFinalQuerySize(search_query_length_)
          .SetResultIndex(result_index_));
}

}  // namespace ash
