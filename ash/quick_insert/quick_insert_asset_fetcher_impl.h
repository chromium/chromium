// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_ASSET_FETCHER_IMPL_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_ASSET_FETCHER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_asset_fetcher.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class GURL;

namespace ash {

class QuickInsertAssetFetcherImplDelegate;

// Implementation of QuickInsertAssetFetcher using a delegate.
class ASH_EXPORT QuickInsertAssetFetcherImpl : public QuickInsertAssetFetcher {
 public:
  // `delegate` must remain valid while this class is alive.
  explicit QuickInsertAssetFetcherImpl(
      QuickInsertAssetFetcherImplDelegate* delegate);
  QuickInsertAssetFetcherImpl(const QuickInsertAssetFetcherImpl&) = delete;
  QuickInsertAssetFetcherImpl& operator=(const QuickInsertAssetFetcherImpl&) =
      delete;
  ~QuickInsertAssetFetcherImpl() override;

  static constexpr size_t kMaxPendingNetworkRequests = 5;

  // QuickInsertAssetFetcher:
  void FetchGifFromUrl(const GURL& url,
                       QuickInsertGifFetchedCallback callback) override;
  void FetchGifPreviewImageFromUrl(
      const GURL& url,
      QuickInsertImageFetchedCallback callback) override;
  void FetchFileThumbnail(const base::FilePath& path,
                          const gfx::Size& size,
                          FetchFileThumbnailCallback callback) override;

 private:
  void OnNetworkRequestCompleted();

  raw_ptr<QuickInsertAssetFetcherImplDelegate> delegate_;
  size_t pending_network_requests_ = 0;
  base::WeakPtrFactory<QuickInsertAssetFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_ASSET_FETCHER_IMPL_H_
