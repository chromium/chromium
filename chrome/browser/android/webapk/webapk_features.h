// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_FEATURES_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

BASE_DECLARE_FEATURE(kWebApkShellUpdate);
extern const base::FeatureParam<int> kWebApkTargetShellVersion;

BASE_DECLARE_FEATURE(kWebApkMinShellVersion);

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_FEATURES_H_
