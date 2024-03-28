// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_clipboard_provider.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"

namespace ash {
namespace {
std::optional<PickerSearchResult::ClipboardData::DisplayFormat>
GetDisplayFormat(crosapi::mojom::ClipboardHistoryDisplayFormat format) {
  switch (format) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile:
      return PickerSearchResult::ClipboardData::DisplayFormat::kFile;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
      return PickerSearchResult::ClipboardData::DisplayFormat::kText;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
      return PickerSearchResult::ClipboardData::DisplayFormat::kImage;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
      return PickerSearchResult::ClipboardData::DisplayFormat::kHtml;
    default:
      return std::nullopt;
  }
}
}

PickerClipboardProvider::PickerClipboardProvider(base::Clock* clock)
    : clock_(clock) {}

PickerClipboardProvider::~PickerClipboardProvider() = default;

void PickerClipboardProvider::FetchResults(OnFetchResultsCallback callback,
                                           base::TimeDelta recency) {
  ash::ClipboardHistoryController* clipboard_history_controller =
      ash::ClipboardHistoryController::Get();
  if (clipboard_history_controller) {
    clipboard_history_controller->GetHistoryValues(base::BindOnce(
        &PickerClipboardProvider::OnFetchHistory,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback), recency));
  }
}

void PickerClipboardProvider::OnFetchHistory(
    OnFetchResultsCallback callback,
    base::TimeDelta recency,
    std::vector<ClipboardHistoryItem> items) {
  std::vector<PickerSearchResult> results;
  for (const auto& item : items) {
    if ((clock_->Now() - item.time_copied()) > recency) {
      continue;
    }
    if (std::optional<PickerSearchResult::ClipboardData::DisplayFormat>
            display_format = GetDisplayFormat(item.display_format());
        display_format.has_value()) {
      results.push_back(PickerSearchResult::Clipboard(
          item.id(), *display_format, item.display_text(),
          item.display_image()));
    }
  }
  std::move(callback).Run(std::move(results));
}
}  // namespace ash
