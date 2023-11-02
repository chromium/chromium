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
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/unguessable_token.h"
#include "ui/base/models/image_model.h"

namespace ash {

class ASH_EXPORT ClipboardHistoryResourceManager
    : public ClipboardHistory::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the CachedImageModel that corresponds with 'menu_item_ids'
    // has been updated.
    virtual void OnCachedImageModelUpdated(
        const std::vector<base::UnguessableToken>& menu_item_ids) = 0;
  };

  explicit ClipboardHistoryResourceManager(
      const ClipboardHistory* clipboard_history);
  ClipboardHistoryResourceManager(const ClipboardHistoryResourceManager&) =
      delete;
  ClipboardHistoryResourceManager& operator=(
      const ClipboardHistoryResourceManager&) = delete;
  ~ClipboardHistoryResourceManager() override;

  // Returns the image to display for the specified clipboard history |item|.
  ui::ImageModel GetImageModel(const ClipboardHistoryItem& item) const;

  // Returns the label to display for the specified clipboard history |item|.
  std::u16string GetLabel(const ClipboardHistoryItem& item) const;

  void AddObserver(Observer* observer) const;
  void RemoveObserver(Observer* observer) const;

 private:
  struct CachedImageModel {
    CachedImageModel();
    CachedImageModel(const CachedImageModel&);
    CachedImageModel& operator=(const CachedImageModel&);
    ~CachedImageModel();
    // Unique identifier.
    base::UnguessableToken id;
    // ImageModel that was created by ClipboardImageModelFactory.
    ui::ImageModel image_model;
    // ClipboardHistoryItem id's which utilize this CachedImageModel.
    std::vector<base::UnguessableToken> clipboard_history_item_ids;
  };

  // Caches the specified |image_model| with the specified |id|.
  void CacheImageModel(const base::UnguessableToken& id,
                       ui::ImageModel image_model);

  // Finds the cached image model associated with the specified |id|.
  std::vector<ClipboardHistoryResourceManager::CachedImageModel>::const_iterator
  FindCachedImageModelForId(const base::UnguessableToken& id) const;

  // Finds the cached image model associated with the specified |item|.
  std::vector<ClipboardHistoryResourceManager::CachedImageModel>::const_iterator
  FindCachedImageModelForItem(const ClipboardHistoryItem& item) const;

  // Cancels all unfinished requests.
  void CancelUnfinishedRequests();

  // ClipboardHistory::Observer:
  void OnClipboardHistoryItemAdded(const ClipboardHistoryItem& item,
                                   bool is_duplicate) override;
  void OnClipboardHistoryItemRemoved(const ClipboardHistoryItem& item) override;
  void OnClipboardHistoryCleared() override;

  // Owned by ClipboardHistoryController.
  const ClipboardHistory* const clipboard_history_;

  std::vector<CachedImageModel> cached_image_models_;

  // Image used when the cached ImageModel has not yet been generated.
  ui::ImageModel placeholder_image_model_;

  // Mutable to allow adding/removing from |observers_| through a const
  // ClipboardHistoryResourceManager.
  mutable base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<ClipboardHistoryResourceManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_RESOURCE_MANAGER_H_
