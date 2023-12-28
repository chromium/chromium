// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_RESOURCE_MANAGER_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_RESOURCE_MANAGER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/unguessable_token.h"
#include "ui/base/models/image_model.h"

namespace ash {

// Helper class that augments certain instances of `ClipboardHistoryItem` with
// asynchronously retrieved metadata.
class ASH_EXPORT ClipboardHistoryResourceManager
    : public ClipboardHistory::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a rendered image model is set on the clipboard history items
    // specified by `menu_item_ids`.
    virtual void OnCachedImageModelUpdated(
        const std::vector<base::UnguessableToken>& menu_item_ids) = 0;
  };

  explicit ClipboardHistoryResourceManager(ClipboardHistory* clipboard_history);
  ClipboardHistoryResourceManager(const ClipboardHistoryResourceManager&) =
      delete;
  ClipboardHistoryResourceManager& operator=(
      const ClipboardHistoryResourceManager&) = delete;
  ~ClipboardHistoryResourceManager() override;

  void AddObserver(Observer* observer) const;
  void RemoveObserver(Observer* observer) const;

 private:
  struct ImageModelRequest {
    ImageModelRequest();
    ImageModelRequest(const ImageModelRequest&);
    ImageModelRequest& operator=(const ImageModelRequest&);
    ~ImageModelRequest();

    // Unique identifier.
    base::UnguessableToken id;

    // IDs of items whose image model will be set to this request's result.
    std::vector<base::UnguessableToken> clipboard_history_item_ids;
  };

  // If `item`'s display text is a URL, queries the primary user profile's
  // browsing history for an associated page title. Asynchronously sets `item`'s
  // secondary display text if a title is found.
  void MaybeQueryUrlTitle(const ClipboardHistoryItem& item);

  // Sets the secondary display text of the `ClipboardHistoryItem` specified by
  // `item_id` with the page title found in the primary user profile's browsing
  // history, if any.
  void OnHistoryQueryComplete(const base::UnguessableToken& item_id,
                              std::optional<std::u16string> maybe_title);

  // Sets `item`'s rendered HTML preview if one is cached; otherwise, ensures
  // that `item` is associated with an asynchronous `ImageModelRequest`.
  void SetOrRequestHtmlPreview(const ClipboardHistoryItem& item);

  // Sets the result `image_model` on each `ClipboardHistoryItem` waiting on the
  // `ImageModelRequest` specified by `id`.
  void OnImageModelRendered(const base::UnguessableToken& id,
                            ui::ImageModel image_model);

  // Finds the pending image model request that `item` is waiting on.
  std::vector<ImageModelRequest>::iterator GetImageModelRequestForItem(
      const ClipboardHistoryItem& item);

  // Cancels all unfinished requests.
  void CancelUnfinishedRequests();

  // ClipboardHistory::Observer:
  void OnClipboardHistoryItemAdded(const ClipboardHistoryItem& item,
                                   bool is_duplicate) override;
  void OnClipboardHistoryItemRemoved(const ClipboardHistoryItem& item) override;
  void OnClipboardHistoryCleared() override;

  // Owned by `ClipboardHistoryController`.
  const raw_ptr<ClipboardHistory> clipboard_history_;

  // Pending requests for image models to be rendered. Once a request finishes,
  // all of the clipboard history items waiting on that image model will be
  // updated, and the request will be removed from this list.
  std::vector<ImageModelRequest> image_model_requests_;

  // Mutable to allow adding/removing from `observers_` through a const
  // `ClipboardHistoryResourceManager`.
  mutable base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<ClipboardHistoryResourceManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_RESOURCE_MANAGER_H_
