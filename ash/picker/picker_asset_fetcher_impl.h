// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_H_
#define ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "base/memory/raw_ptr.h"

class GURL;

namespace ash {

class PickerAssetFetcherImplDelegate;

// Implementation of PickerAssetFetcher using a delegate.
class ASH_EXPORT PickerAssetFetcherImpl : public PickerAssetFetcher {
 public:
  // `delegate` must remain valid while this class is alive.
  explicit PickerAssetFetcherImpl(PickerAssetFetcherImplDelegate* delegate);
  PickerAssetFetcherImpl(const PickerAssetFetcherImpl&) = delete;
  PickerAssetFetcherImpl& operator=(const PickerAssetFetcherImpl&) = delete;
  ~PickerAssetFetcherImpl() override;

  // PickerAssetFetcher:
  void FetchGifFromUrl(const GURL& url,
                       PickerGifFetchedCallback callback) override;
  void FetchGifPreviewImageFromUrl(
      const GURL& url,
      PickerImageFetchedCallback callback) override;
  void FetchFileThumbnail(const base::FilePath& path,
                          const gfx::Size& size,
                          FetchFileThumbnailCallback callback) override;

 private:
  raw_ptr<PickerAssetFetcherImplDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_H_
