// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHILD_MODULE_FEATURES_H_
#define CHROME_BROWSER_CHILD_MODULE_FEATURES_H_

#include "base/feature_list.h"

namespace child_module::features {

// Enables dynamic child module detection and patching.
BASE_DECLARE_FEATURE(kDynamicPatching);

}  // namespace child_module::features

#endif  // CHROME_BROWSER_CHILD_MODULE_FEATURES_H_
