// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/mock_picker_asset_fetcher.h"

#include "ash/picker/picker_asset_fetcher.h"
#include "base/functional/callback.h"
#include "url/gurl.h"

namespace ash {

MockPickerAssetFetcher::MockPickerAssetFetcher() = default;

MockPickerAssetFetcher::~MockPickerAssetFetcher() = default;

void MockPickerAssetFetcher::FetchGifFromUrl(
    const GURL& url,
    PickerGifFetchedCallback callback) {}

void MockPickerAssetFetcher::FetchGifPreviewImageFromUrl(
    const GURL& url,
    PickerImageFetchedCallback callback) {}

void MockPickerAssetFetcher::FetchFileThumbnail(
    const base::FilePath& path,
    const gfx::Size& size,
    FetchFileThumbnailCallback callback) {}

}  // namespace ash
