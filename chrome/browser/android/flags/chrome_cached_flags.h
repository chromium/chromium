// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FLAGS_CHROME_CACHED_FLAGS_H_
#define CHROME_BROWSER_ANDROID_FLAGS_CHROME_CACHED_FLAGS_H_

#include <jni.h>

#include <string>

#include "base/feature_list.h"

namespace chrome {
namespace android {

bool IsJavaDrivenFeatureEnabled(const base::Feature& feature);

// Returns a finch group name currently used for the reached code profiler.
// Returns an empty string if the group isn't specified.
std::string GetReachedCodeProfilerTrialGroup();

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_FLAGS_CHROME_CACHED_FLAGS_H_
