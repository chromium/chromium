// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_data_provider.h"

namespace ash {

BirchDataProvider::BirchDataProvider() = default;

BirchDataProvider::~BirchDataProvider() = default;

void BirchDataProvider::SetDataProviderChangedCallback(
    base::RepeatingClosure callback) {
  CHECK(!data_provider_changed_callback_);
  data_provider_changed_callback_ = std::move(callback);
}

void BirchDataProvider::ResetDataProviderChangedCallback() {
  if (data_provider_changed_callback_) {
    data_provider_changed_callback_.Reset();
  }
}

void BirchDataProvider::NotifyDataProviderChanged() {
  if (data_provider_changed_callback_) {
    data_provider_changed_callback_.Run();
  }
}

}  // namespace ash
