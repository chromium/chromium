// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_clipboard_provider.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"

namespace ash {
namespace {
constexpr base::TimeDelta kRecencyThreshold = base::Seconds(60);

std::optional<PickerSearchResult::ClipboardData::DisplayFormat>
GetDisplayFormat(crosapi::mojom::ClipboardHistoryDisplayFormat format) {
  switch (format) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
      return PickerSearchResult::ClipboardData::DisplayFormat::kText;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
      return PickerSearchResult::ClipboardData::DisplayFormat::kImage;
    default:
      return std::nullopt;
  }
}
}

PickerClipboardProvider::PickerClipboardProvider(base::Clock* clock)
    : clock_(clock) {}

PickerClipboardProvider::~PickerClipboardProvider() = default;

void PickerClipboardProvider::FetchResult(OnFetchResultCallback callback) {
  ash::ClipboardHistoryController* clipboard_history_controller =
      ash::ClipboardHistoryController::Get();
  if (clipboard_history_controller) {
    clipboard_history_controller->GetHistoryValues(
        base::BindOnce(&PickerClipboardProvider::OnFetchHistory,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void PickerClipboardProvider::OnFetchHistory(
    OnFetchResultCallback callback,
    std::vector<ClipboardHistoryItem> items) {
  for (const auto& item : items) {
    if ((clock_->Now() - item.time_copied()) > kRecencyThreshold) {
      continue;
    }
    if (std::optional<PickerSearchResult::ClipboardData::DisplayFormat>
            display_format = GetDisplayFormat(item.display_format());
        display_format.has_value()) {
      callback.Run(PickerSearchResult::Clipboard(item.id(), *display_format,
                                                 item.display_text(),
                                                 item.display_image()));
    }
  }
}

}  // namespace ash
