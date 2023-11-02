// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"

namespace ash {
namespace eche_app {

FakeFeatureStatusProvider::FakeFeatureStatusProvider()
    : FakeFeatureStatusProvider(FeatureStatus::kConnected) {}

FakeFeatureStatusProvider::FakeFeatureStatusProvider(
    FeatureStatus initial_status)
    : status_(initial_status) {}

FakeFeatureStatusProvider::~FakeFeatureStatusProvider() = default;

void FakeFeatureStatusProvider::SetStatus(FeatureStatus status) {
  if (status == status_)
    return;

  status_ = status;
  NotifyStatusChanged();
}

FeatureStatus FakeFeatureStatusProvider::GetStatus() const {
  return status_;
}

}  // namespace eche_app
}  // namespace ash
