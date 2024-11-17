// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/mock_quick_insert_asset_fetcher.h"

#include "ash/quick_insert/quick_insert_asset_fetcher.h"
#include "base/functional/callback.h"
#include "url/gurl.h"

namespace ash {

MockQuickInsertAssetFetcher::MockQuickInsertAssetFetcher() = default;

MockQuickInsertAssetFetcher::~MockQuickInsertAssetFetcher() = default;

void MockQuickInsertAssetFetcher::FetchGifFromUrl(
    const GURL& url,
    QuickInsertGifFetchedCallback callback) {}

void MockQuickInsertAssetFetcher::FetchGifPreviewImageFromUrl(
    const GURL& url,
    QuickInsertImageFetchedCallback callback) {}

void MockQuickInsertAssetFetcher::FetchFileThumbnail(
    const base::FilePath& path,
    const gfx::Size& size,
    FetchFileThumbnailCallback callback) {}

}  // namespace ash
