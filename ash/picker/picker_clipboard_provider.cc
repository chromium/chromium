// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_clipboard_provider.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace {
constexpr base::TimeDelta kRecencyThreshold = base::Seconds(60);
}

PickerClipboardProvider::PickerClipboardProvider(
    SelectSearchResultCallback select_result_callback,
    base::Clock* clock)
    : select_result_callback_(std::move(select_result_callback)),
      clock_(clock) {}

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
    if (item.display_format() ==
        crosapi::mojom::ClipboardHistoryDisplayFormat::kText) {
      auto result = PickerSearchResult::Clipboard(item.id());
      auto item_view = std::make_unique<PickerListItemView>(
          base::BindRepeating(select_result_callback_, result));
      item_view->SetPrimaryText(item.display_text());
      item_view->SetSecondaryText(
          l10n_util::GetStringUTF16(IDS_PICKER_FROM_CLIPBOARD_TEXT));
      item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
          kClipboardIcon, cros_tokens::kCrosSysOnSurface));
      callback.Run(std::move(item_view));
    } else if (item.display_format() ==
               crosapi::mojom::ClipboardHistoryDisplayFormat::kPng) {
      const std::optional<std::vector<uint8_t>>& png_opt =
          item.data().maybe_png();
      if (!png_opt.has_value() || !item.display_image().has_value()) {
        continue;
      }
      auto result = PickerSearchResult::Clipboard(item.id());
      auto item_view = std::make_unique<PickerListItemView>(
          base::BindRepeating(select_result_callback_, result));
      item_view->SetPrimaryImage(
          std::make_unique<views::ImageView>(*item.display_image()));
      item_view->SetSecondaryText(
          l10n_util::GetStringUTF16(IDS_PICKER_FROM_CLIPBOARD_TEXT));
      item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
          kClipboardIcon, cros_tokens::kCrosSysOnSurface));
      callback.Run(std::move(item_view));
    }
  }
}

}  // namespace ash
