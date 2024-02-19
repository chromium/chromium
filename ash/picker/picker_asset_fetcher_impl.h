// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_H_
#define ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class GURL;

namespace ash {

class ASH_EXPORT PickerAssetFetcherImpl : public PickerAssetFetcher {
 public:
  using SharedURLLoaderFactoryGetter =
      base::RepeatingCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

  explicit PickerAssetFetcherImpl(
      SharedURLLoaderFactoryGetter shared_url_loader_factory_getter);
  PickerAssetFetcherImpl(const PickerAssetFetcherImpl&) = delete;
  PickerAssetFetcherImpl& operator=(const PickerAssetFetcherImpl&) = delete;
  ~PickerAssetFetcherImpl() override;

  // PickerAssetFetcher:
  void FetchGifFromUrl(const GURL& url,
                       PickerGifFetchedCallback callback) override;
  void FetchGifPreviewImageFromUrl(
      const GURL& url,
      PickerImageFetchedCallback callback) override;

 private:
  SharedURLLoaderFactoryGetter shared_url_loader_factory_getter_;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_H_
