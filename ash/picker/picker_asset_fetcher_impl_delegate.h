// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_DELEGATE_H_
#define ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

class ASH_EXPORT PickerAssetFetcherImplDelegate {
 public:
  virtual ~PickerAssetFetcherImplDelegate() = default;

  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSharedURLLoaderFactory() = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_ASSET_FETCHER_IMPL_DELEGATE_H_
