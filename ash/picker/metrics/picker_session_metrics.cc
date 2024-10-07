// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_session_metrics.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_search_result.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/text_input_client.h"

namespace ash {
namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

constexpr int kCapsLockCountThreshold = 20;

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
    case PickerSessionMetrics::SessionOutcome::kOpenFile:
      return cros_events::PickerSessionOutcome::OPEN_FILE;
    case PickerSessionMetrics::SessionOutcome::kOpenLink:
      return cros_events::PickerSessionOutcome::OPEN_LINK;
    case PickerSessionMetrics::SessionOutcome::kCreate:
      return cros_events::PickerSessionOutcome::CREATE;
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
    case PickerCategory::kLobster:
      return cros_events::PickerAction::OPEN_LOBSTER;
    case PickerCategory::kLinks:
      return cros_events::PickerAction::OPEN_LINKS;
    case PickerCategory::kEmojisGifs:
    case PickerCategory::kEmojis:
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
          [](const PickerTextResult& data) {
            switch (data.source) {
              case PickerTextResult::Source::kUnknown:
                return cros_events::PickerResultSource::UNKNOWN;
              case PickerTextResult::Source::kDate:
                return cros_events::PickerResultSource::DATES_TIMES;
              case PickerTextResult::Source::kMath:
                return cros_events::PickerResultSource::UNITS_MATHS;
              case PickerTextResult::Source::kCaseTransform:
                return cros_events::PickerResultSource::CASE_TRANSFORM;
              case PickerTextResult::Source::kOmnibox:
                return cros_events::PickerResultSource::OMNIBOX;
            }
          },
          [](const PickerEmojiResult& data) {
            return cros_events::PickerResultSource::EMOJI;
          },
          [](const PickerClipboardResult& data) {
            return cros_events::PickerResultSource::CLIPBOARD;
          },
          [](const PickerGifResult& data) {
            return cros_events::PickerResultSource::TENOR;
          },
          [](const PickerBrowsingHistoryResult& data) {
            return cros_events::PickerResultSource::OMNIBOX;
          },
          [](const PickerLocalFileResult& data) {
            return cros_events::PickerResultSource::LOCAL_FILES;
          },
          [](const PickerDriveFileResult& data) {
            return cros_events::PickerResultSource::DRIVE_FILES;
          },
          [](const PickerCategoryResult& data) -> ReturnType { NOTREACHED(); },
          [](const PickerSearchRequestResult& data) -> ReturnType {
            NOTREACHED();
          },
          [](const PickerEditorResult& data) -> ReturnType { NOTREACHED(); },
          [](const PickerLobsterResult& data) -> ReturnType { NOTREACHED(); },
          [](const PickerNewWindowResult& data) -> ReturnType {
            return cros_events::PickerResultSource::UNKNOWN;
          },
          [](const PickerCapsLockResult& data) -> ReturnType {
            return cros_events::PickerResultSource::UNKNOWN;
          },
          [](const PickerCaseTransformResult& data) -> ReturnType {
            return cros_events::PickerResultSource::CASE_TRANSFORM;
          },
      },
      *result);
}

cros_events::PickerResultType GetResultType(
    std::optional<PickerSearchResult> result) {
  if (!result.has_value()) {
    return cros_events::PickerResultType::UNKNOWN;
  }
  using ReturnType = cros_events::PickerResultType;
  return std::visit(
      base::Overloaded{
          [](const PickerTextResult& data) {
            return cros_events::PickerResultType::TEXT;
          },
          [](const PickerEmojiResult& data) {
            switch (data.type) {
              case PickerEmojiResult::Type::kEmoji:
                return cros_events::PickerResultType::EMOJI;
              case PickerEmojiResult::Type::kSymbol:
                return cros_events::PickerResultType::SYMBOL;
              case PickerEmojiResult::Type::kEmoticon:
                return cros_events::PickerResultType::EMOTICON;
            }
          },
          [](const PickerClipboardResult& data) {
            switch (data.display_format) {
              case PickerClipboardResult::DisplayFormat::kFile:
                return cros_events::PickerResultType::CLIPBOARD_FILE;
              case PickerClipboardResult::DisplayFormat::kText:
                return cros_events::PickerResultType::CLIPBOARD_TEXT;
              case PickerClipboardResult::DisplayFormat::kImage:
                return cros_events::PickerResultType::CLIPBOARD_IMAGE;
              case PickerClipboardResult::DisplayFormat::kHtml:
                return cros_events::PickerResultType::CLIPBOARD_HTML;
            }
          },
          [](const PickerGifResult& data) {
            return cros_events::PickerResultType::GIF;
          },
          [](const PickerBrowsingHistoryResult& data) {
            return cros_events::PickerResultType::LINK;
          },
          [](const PickerLocalFileResult& data) {
            return cros_events::PickerResultType::LOCAL_FILE;
          },
          [](const PickerDriveFileResult& data) {
            return cros_events::PickerResultType::DRIVE_FILE;
          },
          [](const PickerCategoryResult& data) -> ReturnType { NOTREACHED(); },
          [](const PickerSearchRequestResult& data) -> ReturnType {
            NOTREACHED();
          },
          [](const PickerEditorResult& data) -> ReturnType { NOTREACHED(); },
          [](const PickerLobsterResult& data) -> ReturnType { NOTREACHED(); },
          [](const PickerNewWindowResult& data) -> ReturnType {
            return cros_events::PickerResultType::UNKNOWN;
          },
          [](const PickerCapsLockResult& data) -> ReturnType {
            return cros_events::PickerResultType::UNKNOWN;
          },
          [](const PickerCaseTransformResult& data) -> ReturnType {
            return cros_events::PickerResultType::TEXT;
          },
      },
      *result);
}

}  // namespace

PickerSessionMetrics::PickerSessionMetrics() = default;

PickerSessionMetrics::PickerSessionMetrics(PrefService* prefs)
    : prefs_(prefs) {}

PickerSessionMetrics::~PickerSessionMetrics() {
  OnFinishSession();
}

void PickerSessionMetrics::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kPickerCapsLockSelectedCountPrefName, 0);
  registry->RegisterIntegerPref(prefs::kPickerCapsLockDislayedCountPrefName, 0);
}

void PickerSessionMetrics::SetOutcome(SessionOutcome outcome) {
  if (outcome_ == SessionOutcome::kUnknown) {
    outcome_ = outcome;
  }
}

void PickerSessionMetrics::SetSelectedCategory(PickerCategory category) {
  if (!last_category_.has_value()) {
    last_category_ = category;
  }
}

void PickerSessionMetrics::SetSelectedResult(PickerSearchResult selected_result,
                                             int index) {
  if (!selected_result_.has_value()) {
    selected_result_ = std::move(selected_result);
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
  if (caps_lock_displayed_) {
    UpdateCapLockPrefs(
        selected_result_.has_value() &&
        std::holds_alternative<PickerCapsLockResult>(*selected_result_));
  }
  base::UmaHistogramEnumeration("Ash.Picker.Session.Outcome", outcome_);
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::Picker_FinishSession()
          .SetOutcome(ConvertToCrosEventSessionOutcome(outcome_))
          .SetAction(ConvertToCrosEventAction(last_category_))
          .SetResultSource(GetResultSource(std::move(selected_result_)))
          .SetResultType(GetResultType(std::move(selected_result_)))
          .SetTotalEdits(search_query_total_edits_)
          .SetFinalQuerySize(search_query_length_)
          .SetResultIndex(result_index_));
}

void PickerSessionMetrics::SetCapsLockDisplayed(bool displayed) {
  caps_lock_displayed_ = displayed;
}

void PickerSessionMetrics::UpdateCapLockPrefs(bool caps_lock_selected) {
  if (prefs_ == nullptr) {
    return;
  }
  int caps_lock_displayed_count =
      prefs_->GetInteger(prefs::kPickerCapsLockDislayedCountPrefName) + 1;
  int caps_lock_selected_count =
      prefs_->GetInteger(prefs::kPickerCapsLockSelectedCountPrefName);
  if (caps_lock_selected) {
    ++caps_lock_selected_count;
  }
  // We will only use caps_lock_selected_count / caps_lock_displayed_count to
  // decide the position of caps lock toggle. We halves both numbers so that
  // they don't grow infinitely and later usages have more weights in decision
  // making. The remainders in division is not significant in our use cases.
  if (caps_lock_displayed_count >= kCapsLockCountThreshold) {
    caps_lock_displayed_count /= 2;
    caps_lock_selected_count /= 2;
  }
  prefs_->SetInteger(prefs::kPickerCapsLockDislayedCountPrefName,
                     caps_lock_displayed_count);
  prefs_->SetInteger(prefs::kPickerCapsLockSelectedCountPrefName,
                     caps_lock_selected_count);
}

}  // namespace ash
