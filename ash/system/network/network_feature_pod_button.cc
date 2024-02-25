// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_pod_button.h"

#include "base/check.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

NetworkFeaturePodButton::NetworkFeaturePodButton(
    FeaturePodControllerBase* controller,
    Delegate* delegate)
    : FeaturePodButton(controller), delegate_(delegate) {
  DCHECK(delegate);
}

NetworkFeaturePodButton::~NetworkFeaturePodButton() = default;

void NetworkFeaturePodButton::OnThemeChanged() {
  FeaturePodButton::OnThemeChanged();
  delegate_->OnFeaturePodButtonThemeChanged();
}

BEGIN_METADATA(NetworkFeaturePodButton)
END_METADATA

}  // namespace ash
