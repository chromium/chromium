// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_provider.h"

namespace ash {

BirchCoralProvider::BirchCoralProvider(BirchModel* birch_model)
    : birch_model_(birch_model) {}
BirchCoralProvider::~BirchCoralProvider() = default;

void BirchCoralProvider::RequestBirchDataFetch() {
  // TODO(yulunwu) fetch coral data and update `birch_model_`
}

}  // namespace ash
