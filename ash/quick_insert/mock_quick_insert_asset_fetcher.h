// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_MOCK_QUICK_INSERT_ASSET_FETCHER_H_
#define ASH_QUICK_INSERT_MOCK_QUICK_INSERT_ASSET_FETCHER_H_

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_asset_fetcher.h"

namespace ash {

class ASH_EXPORT MockPickerAssetFetcher : public PickerAssetFetcher {
 public:
  MockPickerAssetFetcher();
  MockPickerAssetFetcher(const MockPickerAssetFetcher&) = delete;
  MockPickerAssetFetcher& operator=(const MockPickerAssetFetcher&) = delete;
  ~MockPickerAssetFetcher() override;

  // PickerAssetFetcher:
  void FetchGifFromUrl(const GURL& url,
                       PickerGifFetchedCallback callback) override;
  void FetchGifPreviewImageFromUrl(
      const GURL& url,
      PickerImageFetchedCallback callback) override;
  void FetchFileThumbnail(const base::FilePath& path,
                          const gfx::Size& size,
                          FetchFileThumbnailCallback callback) override;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_MOCK_QUICK_INSERT_ASSET_FETCHER_H_
