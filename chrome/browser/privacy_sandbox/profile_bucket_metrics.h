// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PROFILE_BUCKET_METRICS_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PROFILE_BUCKET_METRICS_H_

#include <string>

class Profile;

namespace privacy_sandbox {

// Returns the profile bucket associated to a profile used for metrics
// tracking.
// TODO(crbug.com/369440570): Add tests for this function.
std::string GetProfileBucketName(Profile* profile);

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PROFILE_BUCKET_METRICS_H_
