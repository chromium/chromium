// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SCOPED_ADD_FEATURE_FLAGS_H_
#define ANDROID_WEBVIEW_BROWSER_SCOPED_ADD_FEATURE_FLAGS_H_

#include <string>
#include <vector>

#include "base/feature_list.h"

namespace base {
class CommandLine;
}

namespace android_webview {

class ScopedAddFeatureFlags {
 public:
  explicit ScopedAddFeatureFlags(base::CommandLine* cl);
  ~ScopedAddFeatureFlags();

  // Any existing (user set) enable/disable takes precedence.
  void EnableIfNotSet(const base::Feature& feature);
  void DisableIfNotSet(const base::Feature& feature);
  // Check if the feature is enabled from command line or functions above
  bool IsEnabled(const base::Feature& feature);

 private:
  void AddFeatureIfNotSet(const base::Feature& feature, bool enable);

  base::CommandLine* const cl_;
  std::vector<std::string> enabled_features_;
  std::vector<std::string> disabled_features_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAddFeatureFlags);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SCOPED_ADD_FEATURE_FLAGS_H_
