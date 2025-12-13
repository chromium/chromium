// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/webstore/features.h"

#include "base/feature_list.h"

namespace enterprise::webstore {

BASE_FEATURE(kChromeWebStoreNavigationThrottle,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace enterprise::webstore
