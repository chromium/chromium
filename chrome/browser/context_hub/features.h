// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_FEATURES_H_
#define CHROME_BROWSER_CONTEXT_HUB_FEATURES_H_

#include "base/feature_list.h"

namespace context_hub::features {

// The main feature flag for the Context Hub service. When disabled,
// all Context Hub features and services are turned off.
BASE_DECLARE_FEATURE(kContextHub);

}  // namespace context_hub::features

#endif  // CHROME_BROWSER_CONTEXT_HUB_FEATURES_H_
