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

namespace network {
class SimpleURLLoader;
}

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

  // QuickInsertAssetFetcher:
  std::unique_ptr<network::SimpleURLLoader> FetchGifFromUrl(
      const GURL& url,
      size_t rank,
      QuickInsertGifFetchedCallback callback) override;
  std::unique_ptr<network::SimpleURLLoader> FetchGifPreviewImageFromUrl(
      const GURL& url,
      size_t rank,
      QuickInsertImageFetchedCallback callback) override;
  void FetchFileThumbnail(const base::FilePath& path,
                          const gfx::Size& size,
                          FetchFileThumbnailCallback callback) override;

 private:
  raw_ptr<QuickInsertAssetFetcherImplDelegate> delegate_;
  base::WeakPtrFactory<QuickInsertAssetFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_ASSET_FETCHER_IMPL_H_
