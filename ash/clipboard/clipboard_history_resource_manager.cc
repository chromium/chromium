// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_resource_manager.h"

#include <string>
#include <vector>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_url_title_fetcher.h"
#include "ash/constants/ash_features.h"
#include "ash/display/display_util.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/display/screen.h"

namespace ash {

// ClipboardHistoryResourceManager ---------------------------------------------

ClipboardHistoryResourceManager::ClipboardHistoryResourceManager(
    ClipboardHistory* clipboard_history)
    : clipboard_history_(clipboard_history) {
  clipboard_history_->AddObserver(this);
}

ClipboardHistoryResourceManager::~ClipboardHistoryResourceManager() {
  clipboard_history_->RemoveObserver(this);
  if (ClipboardImageModelFactory::Get())
    ClipboardImageModelFactory::Get()->OnShutdown();
}

void ClipboardHistoryResourceManager::AddObserver(Observer* observer) const {
  observers_.AddObserver(observer);
}

void ClipboardHistoryResourceManager::RemoveObserver(Observer* observer) const {
  observers_.RemoveObserver(observer);
}

ClipboardHistoryResourceManager::ImageModelRequest::ImageModelRequest() =
    default;

ClipboardHistoryResourceManager::ImageModelRequest::ImageModelRequest(
    const ImageModelRequest& other) = default;

ClipboardHistoryResourceManager::ImageModelRequest&
ClipboardHistoryResourceManager::ImageModelRequest::operator=(
    const ImageModelRequest&) = default;

ClipboardHistoryResourceManager::ImageModelRequest::~ImageModelRequest() =
    default;

void ClipboardHistoryResourceManager::MaybeQueryUrlTitle(
    const ClipboardHistoryItem& item) {
  // `url_title_fetcher` may be null in tests.
  if (auto* const url_title_fetcher = ClipboardHistoryUrlTitleFetcher::Get();
      url_title_fetcher &&
      chromeos::clipboard_history::IsUrl(item.display_text())) {
    url_title_fetcher->QueryHistory(
        GURL(item.display_text()),
        base::BindOnce(&ClipboardHistoryResourceManager::OnHistoryQueryComplete,
                       weak_factory_.GetWeakPtr(), item.id()));
  }
}

void ClipboardHistoryResourceManager::OnHistoryQueryComplete(
    const base::UnguessableToken& item_id,
    std::optional<std::u16string> maybe_title) {
  auto& items = clipboard_history_->GetItems();
  auto item = base::ranges::find(items, item_id, &ClipboardHistoryItem::id);
  if (item == items.end()) {
    return;
  }

  if (maybe_title) {
    base::TrimWhitespace(*maybe_title, base::TRIM_ALL, &(*maybe_title));
    if (maybe_title->empty()) {
      // If the retrieved title was empty or consisted of only whitespace, the
      // item has nothing to display as secondary text.
      maybe_title.reset();
    }
  }
  item->set_secondary_display_text(maybe_title);
}

void ClipboardHistoryResourceManager::SetOrRequestHtmlPreview(
    const ClipboardHistoryItem& item) {
  auto& items = clipboard_history_->GetItems();

  // See if we have an `existing` item that will render the same as `item`.
  auto it = base::ranges::find_if(items, [&](const auto& existing) {
    return &existing != &item &&
           existing.display_format() ==
               crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml &&
           existing.data().markup_data() == item.data().markup_data();
  });

  // If no existing item will render the same as `item`, create a new request to
  // render an HTML preview for `item`. Note that the image model factory may
  // or may not start rendering immediately depending on its activation status.
  if (it == items.end()) {
    base::UnguessableToken id = base::UnguessableToken::Create();
    ImageModelRequest image_model_request;
    image_model_request.id = id;
    image_model_request.clipboard_history_item_ids.push_back(item.id());
    image_model_requests_.push_back(std::move(image_model_request));

    // `image_model_factory` can be nullptr in tests.
    auto* image_model_factory = ClipboardImageModelFactory::Get();
    if (!image_model_factory) {
      return;
    }

    // `text_input_client` can be nullptr in tests.
    const auto* text_input_client =
        ash::GetWindowTreeHostForDisplay(
            display::Screen::GetScreen()->GetPrimaryDisplay().id())
            ->GetInputMethod()
            ->GetTextInputClient();
    const gfx::Rect bounding_box =
        text_input_client ? text_input_client->GetSelectionBoundingBox()
                          : gfx::Rect();

    image_model_factory->Render(
        id, item.data().markup_data(),
        IsRectContainedByAnyDisplay(bounding_box) ? bounding_box.size()
                                                  : gfx::Size(),
        base::BindOnce(&ClipboardHistoryResourceManager::OnImageModelRendered,
                       weak_factory_.GetWeakPtr(), id));
    return;
  }

  // If there is an existing item that will render the same as `item`, check
  // whether the existing item's preview has rendered.
  auto image_model_request = GetImageModelRequestForItem(*it);
  if (image_model_request != image_model_requests_.end()) {
    // If rendering is still in progress, just note that `item` will need to
    // hear about the result as well.
    image_model_request->clipboard_history_item_ids.push_back(item.id());
  } else {
    // If rendering has finished, set `item` to have the same preview.
    auto mutable_item =
        base::ranges::find(items, item.id(), &ClipboardHistoryItem::id);
    DCHECK(mutable_item != items.end());

    const auto& existing_preview = it->display_image();
    DCHECK(existing_preview.has_value());

    mutable_item->SetDisplayImage(existing_preview.value());
  }
}

void ClipboardHistoryResourceManager::OnImageModelRendered(
    const base::UnguessableToken& id,
    ui::ImageModel image_model) {
  auto image_model_request = base::ranges::find(
      image_model_requests_, id,
      &ClipboardHistoryResourceManager::ImageModelRequest::id);
  if (image_model_request == image_model_requests_.end()) {
    return;
  }

  // Set the HTML preview for each item attached to `id`'s request.
  for (auto& item : clipboard_history_->GetItems()) {
    if (!base::Contains(image_model_request->clipboard_history_item_ids,
                        item.id())) {
      continue;
    }

    DCHECK(item.display_image().has_value());
    if (item.display_image().value() != image_model) {
      item.SetDisplayImage(image_model);
    }
  }

  for (auto& observer : observers_) {
    observer.OnCachedImageModelUpdated(
        image_model_request->clipboard_history_item_ids);
  }

  image_model_requests_.erase(image_model_request);
}

void ClipboardHistoryResourceManager::CancelUnfinishedRequests() {
  for (const auto& image_model_request : image_model_requests_) {
    ClipboardImageModelFactory::Get()->CancelRequest(image_model_request.id);
  }
}

std::vector<ClipboardHistoryResourceManager::ImageModelRequest>::iterator
ClipboardHistoryResourceManager::GetImageModelRequestForItem(
    const ClipboardHistoryItem& item) {
  return base::ranges::find_if(
      image_model_requests_, [&](const auto& image_model_request) {
        return base::Contains(image_model_request.clipboard_history_item_ids,
                              item.id());
      });
}

void ClipboardHistoryResourceManager::OnClipboardHistoryItemAdded(
    const ClipboardHistoryItem& item,
    bool is_duplicate) {
  if (item.display_format() ==
          crosapi::mojom::ClipboardHistoryDisplayFormat::kText &&
      features::IsClipboardHistoryUrlTitlesEnabled()) {
    // An item being re-copied might need its URL title changed based on updates
    // to the user's browsing history.
    MaybeQueryUrlTitle(item);
  } else if (item.display_format() ==
                 crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml &&
             !is_duplicate) {
    // If an item is being copied for the first time, we begin rendering its
    // HTML preview as soon as possible.
    SetOrRequestHtmlPreview(item);
  }
}

void ClipboardHistoryResourceManager::OnClipboardHistoryItemRemoved(
    const ClipboardHistoryItem& item) {
  // For items that will not be represented by their rendered HTML, do nothing.
  if (item.display_format() !=
      crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml) {
    return;
  }

  // If the item's image model request has already finished, there is nothing
  // more to do.
  auto image_model_request = GetImageModelRequestForItem(item);
  if (image_model_request == image_model_requests_.end()) {
    return;
  }

  // If `item` was attached to a pending request, make sure it is not updated
  // when rendering finishes.
  std::erase(image_model_request->clipboard_history_item_ids, item.id());

  if (image_model_request->clipboard_history_item_ids.empty()) {
    // If no more items are waiting on the image model, cancel the request.
    ClipboardImageModelFactory::Get()->CancelRequest(image_model_request->id);
    image_model_requests_.erase(image_model_request);
  }
}

void ClipboardHistoryResourceManager::OnClipboardHistoryCleared() {
  CancelUnfinishedRequests();
  image_model_requests_.clear();
}

}  // namespace ash
