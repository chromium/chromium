// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_COLOR_PROVIDER_IMPL_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_COLOR_PROVIDER_IMPL_H_

#include "ash/public/cpp/holding_space/holding_space_color_provider.h"

namespace ash {

// The implementation of the singleton which provides colors to holding space.
class HoldingSpaceColorProviderImpl : public HoldingSpaceColorProvider {
 public:
  HoldingSpaceColorProviderImpl();
  HoldingSpaceColorProviderImpl(const HoldingSpaceColorProviderImpl&) = delete;
  HoldingSpaceColorProviderImpl& operator=(
      const HoldingSpaceColorProviderImpl&) = delete;
  ~HoldingSpaceColorProviderImpl() override;

 private:
  // HoldingSpaceColorProvider:
  SkColor GetBackgroundColor() const override;
  SkColor GetFileIconColor() const override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_COLOR_PROVIDER_H_
