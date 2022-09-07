// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

void FeaturePodControllerBase::OnLabelPressed() {
  return OnIconPressed();
}

}  // namespace ash
