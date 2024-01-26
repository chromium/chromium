// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_asset_fetcher_impl.h"

#include <utility>

#include "ash/picker/picker_asset_fetcher.h"
#include "ash/public/cpp/image_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "url/gurl.h"

namespace ash {

PickerAssetFetcherImpl::PickerAssetFetcherImpl(GifUrlLoader gif_url_loader)
    : gif_url_loader_(std::move(gif_url_loader)) {}

PickerAssetFetcherImpl::~PickerAssetFetcherImpl() = default;

void PickerAssetFetcherImpl::FetchGifFromUrl(
    const GURL& url,
    PickerGifFetchedCallback callback) {
  gif_url_loader_.Run(url, base::BindOnce(&image_util::DecodeAnimationData,
                                          std::move(callback)));
}

}  // namespace ash
