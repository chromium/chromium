// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_ASSET_FETCHER_IMPL_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_ASSET_FETCHER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_asset_fetcher.h"
#include "base/memory/raw_ptr.h"

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
  raw_ptr<QuickInsertAssetFetcherImplDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_ASSET_FETCHER_IMPL_H_
