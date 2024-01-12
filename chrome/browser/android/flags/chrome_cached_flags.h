// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FLAGS_CHROME_CACHED_FLAGS_H_
#define CHROME_BROWSER_ANDROID_FLAGS_CHROME_CACHED_FLAGS_H_

#include "base/feature_list.h"

namespace chrome::android {

bool IsJavaDrivenFeatureEnabled(const base::Feature& feature);

}  // namespace chrome::android

#endif  // CHROME_BROWSER_ANDROID_FLAGS_CHROME_CACHED_FLAGS_H_
