// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_repository.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "ash/services/quick_pair/public/cpp/account_key_filter.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

FastPairRepository* g_instance = nullptr;

// static
FastPairRepository* FastPairRepository::Get() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void FastPairRepository::SetInstance(FastPairRepository* instance) {
  DCHECK(!g_instance || !instance);
  g_instance = instance;
}

FastPairRepository::FastPairRepository() {
  SetInstance(this);
}

FastPairRepository::~FastPairRepository() {
  SetInstance(nullptr);
}

}  // namespace quick_pair
}  // namespace ash
