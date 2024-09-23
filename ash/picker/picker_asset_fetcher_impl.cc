// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_asset_fetcher_impl.h"

#include <utility>

#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_asset_fetcher_impl_delegate.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

PickerAssetFetcherImpl::PickerAssetFetcherImpl(
    PickerAssetFetcherImplDelegate* delegate)
    : delegate_(delegate) {}

PickerAssetFetcherImpl::~PickerAssetFetcherImpl() = default;

void PickerAssetFetcherImpl::FetchFileThumbnail(
    const base::FilePath& path,
    const gfx::Size& size,
    FetchFileThumbnailCallback callback) {
  delegate_->FetchFileThumbnail(path, size, std::move(callback));
}

}  // namespace ash
