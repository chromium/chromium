// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_DOWNLOADS_OFFLINE_PAGE_SHARE_HELPER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_DOWNLOADS_OFFLINE_PAGE_SHARE_HELPER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_pages/core/offline_page_types.h"

namespace offline_pages {

class OfflinePageModel;

// Helper class to retrieve the offline page content URI for sharing the page
// in download home UI.
class OfflinePageShareHelper {
 public:
  using ContentId = offline_items_collection::ContentId;
  using OfflineItemShareInfo = offline_items_collection::OfflineItemShareInfo;
  using ResultCallback =
      base::OnceCallback<void(ShareResult,
                              const ContentId&,
                              std::unique_ptr<OfflineItemShareInfo>)>;

  explicit OfflinePageShareHelper(OfflinePageModel* model);
  ~OfflinePageShareHelper();

  // Get the share info. Mainly to retrieve the content URI.
  void GetShareInfo(const ContentId& id, ResultCallback result_cb);

 private:
  void OnPageGetForShare(const std::vector<OfflinePageItem>& pages);

  void AcquireFileAccessPermission();
  void OnFileAccessPermissionDone(bool granted);

  void OnPageGetForPublish(const std::vector<OfflinePageItem>& pages);
  void OnPagePublished(const base::FilePath& file_path, SavePageResult result);

  void NotifyCompletion(ShareResult result,
                        std::unique_ptr<OfflineItemShareInfo> share_info);

  // A keyed service, always valid.
  OfflinePageModel* model_;

  ResultCallback result_cb_;
  ContentId content_id_;

  base::WeakPtrFactory<OfflinePageShareHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OfflinePageShareHelper);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_DOWNLOADS_OFFLINE_PAGE_SHARE_HELPER_H_
