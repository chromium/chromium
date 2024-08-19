// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_clipboard_history_provider.h"

#include <string>
#include <string_view>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/i18n/case_conversion.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr base::TimeDelta kRecencyThreshold = base::Seconds(60);

std::optional<PickerSearchResult::ClipboardData::DisplayFormat>
GetDisplayFormat(const ClipboardHistoryItem& item) {
  switch (item.display_format()) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile:
      return PickerSearchResult::ClipboardData::DisplayFormat::kFile;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
      return GURL(item.display_text()).is_valid()
                 ? PickerSearchResult::ClipboardData::DisplayFormat::kUrl
                 : PickerSearchResult::ClipboardData::DisplayFormat::kText;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
      return PickerSearchResult::ClipboardData::DisplayFormat::kImage;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
      // TODO: b/348102522 - Show HTML content once it's possible to render them
      // inside Picker.
    default:
      return std::nullopt;
  }
}

bool MatchQuery(const ClipboardHistoryItem& item, std::u16string_view query) {
  if (query.empty()) {
    return true;
  }
  if (item.display_format() !=
          crosapi::mojom::ClipboardHistoryDisplayFormat::kText &&
      item.display_format() !=
          crosapi::mojom::ClipboardHistoryDisplayFormat::kFile) {
    return false;
  }
  return base::i18n::ToLower(item.display_text())
             .find(base::i18n::ToLower(query)) != std::u16string::npos;
}
}  // namespace

PickerClipboardHistoryProvider::PickerClipboardHistoryProvider(
    base::Clock* clock)
    : clock_(clock) {}

PickerClipboardHistoryProvider::~PickerClipboardHistoryProvider() = default;

void PickerClipboardHistoryProvider::FetchResults(
    OnFetchResultsCallback callback,
    std::u16string_view query) {
  ash::ClipboardHistoryController* clipboard_history_controller =
      ash::ClipboardHistoryController::Get();
  if (clipboard_history_controller) {
    clipboard_history_controller->GetHistoryValues(
        base::BindOnce(&PickerClipboardHistoryProvider::OnFetchHistory,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::u16string(query)));
  }
}

void PickerClipboardHistoryProvider::OnFetchHistory(
    OnFetchResultsCallback callback,
    std::u16string query,
    std::vector<ClipboardHistoryItem> items) {
  std::vector<PickerSearchResult> results;
  for (const auto& item : items) {
    if (!MatchQuery(item, query)) {
      continue;
    }
    if (std::optional<PickerSearchResult::ClipboardData::DisplayFormat>
            display_format = GetDisplayFormat(item);
        display_format.has_value()) {
      results.push_back(PickerSearchResult::Clipboard(
          item.id(), *display_format, item.file_count(), item.display_text(),
          item.display_image(),
          (clock_->Now() - item.time_copied()) < kRecencyThreshold));
    }
  }
  std::move(callback).Run(std::move(results));
}
}  // namespace ash
