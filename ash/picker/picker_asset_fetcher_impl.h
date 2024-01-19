// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_H_
#define ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "base/functional/callback.h"

class GURL;

namespace ash {

class ASH_EXPORT PickerAssetFetcherImpl : public PickerAssetFetcher {
 public:
  using GifDataLoadedCallback =
      base::OnceCallback<void(const std::string& gif_data)>;
  using GifUrlLoader =
      base::RepeatingCallback<void(const GURL& url,
                                   GifDataLoadedCallback callback)>;

  explicit PickerAssetFetcherImpl(GifUrlLoader gif_url_loader);
  PickerAssetFetcherImpl(const PickerAssetFetcherImpl&) = delete;
  PickerAssetFetcherImpl& operator=(const PickerAssetFetcherImpl&) = delete;
  ~PickerAssetFetcherImpl() override;

  // PickerAssetFetcher:
  void FetchGifFromUrl(const GURL& url,
                       PickerGifFetchedCallback callback) override;

 private:
  // Helper for loading gifs as encoded strings from urls.
  GifUrlLoader gif_url_loader_;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_H_
