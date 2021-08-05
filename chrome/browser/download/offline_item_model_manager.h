// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_H_

#include <memory>

#include "chrome/browser/download/offline_item_model_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_items_collection/core/offline_item.h"

namespace content {
class BrowserContext;
}  // namespace content

using ContentId = offline_items_collection::ContentId;

// Class for managing all the OfflineModels for a profile.
class OfflineItemModelManager : public KeyedService {
 public:
  // Constructs a OfflineItemModelManager.
  explicit OfflineItemModelManager(content::BrowserContext* browser_context);
  ~OfflineItemModelManager() override;

  // Returns the OfflineItemModel for the ContentId, if not found, an empty
  // OfflineItemModel will be created and returned.
  OfflineItemModelData* GetOrCreateOfflineItemModelData(const ContentId& id);

  void RemoveOfflineItemModelData(const ContentId& id);

  content::BrowserContext* browser_context() { return browser_context_; }

 private:
  content::BrowserContext* browser_context_;
  std::map<ContentId, std::unique_ptr<OfflineItemModelData>>
      offline_item_model_data_;

  DISALLOW_COPY_AND_ASSIGN(OfflineItemModelManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_H_
