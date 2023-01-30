// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_FEATURES_H_
#define CC_SLIM_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace features {

COMPONENT_EXPORT(CC_SLIM) BASE_DECLARE_FEATURE(kSlimCompositor);

COMPONENT_EXPORT(CC_SLIM) bool IsSlimCompositorEnabled();

}  // namespace features

#endif  // CC_SLIM_FEATURES_H_
