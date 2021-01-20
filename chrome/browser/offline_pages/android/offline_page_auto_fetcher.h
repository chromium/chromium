// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace offline_pages {
class OfflinePageAutoFetcherService;

// Provides control of fetching pages for later in an automatic / quiet way.
class OfflinePageAutoFetcher : public chrome::mojom::OfflinePageAutoFetcher {
 public:
  explicit OfflinePageAutoFetcher(content::RenderFrameHost* render_frame_host);
  ~OfflinePageAutoFetcher() override;
  void TrySchedule(bool user_requested, TryScheduleCallback callback) override;
  void CancelSchedule() override;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<chrome::mojom::OfflinePageAutoFetcher> receiver);

 private:
  OfflinePageAutoFetcherService* GetService();

  using OfflinePageAutoFetcherScheduleResult =
      chrome::mojom::OfflinePageAutoFetcherScheduleResult;

  GURL last_committed_url_;
  int android_tab_id_;
  OfflinePageAutoFetcherService* auto_fetcher_service_ = nullptr;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_H_
