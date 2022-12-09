// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_EMPTY_COMPONENT_LOADER_POLICY_H_
#define ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_EMPTY_COMPONENT_LOADER_POLICY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "components/component_updater/android/component_loader_policy.h"

namespace base {
class Version;
}  // namespace base

namespace android_webview {

// A fake empty component to run experiment to measure component updater
// performance impact.
// TODO(crbug.com/1288006): remove this when the experiment is over.
class EmptyComponentLoaderPolicy
    : public component_updater::ComponentLoaderPolicy {
 public:
  EmptyComponentLoaderPolicy() = default;
  ~EmptyComponentLoaderPolicy() override = default;

  EmptyComponentLoaderPolicy(const EmptyComponentLoaderPolicy&) = delete;
  EmptyComponentLoaderPolicy& operator=(const EmptyComponentLoaderPolicy&) =
      delete;

  // The following methods override ComponentLoaderPolicy.
  void ComponentLoaded(const base::Version& version,
                       base::flat_map<std::string, base::ScopedFD>& fd_map,
                       base::Value::Dict manifest) override;
  void ComponentLoadFailed(
      component_updater::ComponentLoadResult error) override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetMetricsSuffix() const override;
};

void LoadEmptyComponent(
    component_updater::ComponentLoaderPolicyVector& policies);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_EMPTY_COMPONENT_LOADER_POLICY_H_
