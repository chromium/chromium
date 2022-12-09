// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/loader_policies/empty_component_loader_policy.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/common/aw_features.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"

namespace android_webview {

namespace {

// Persisted to logs, should never change.
constexpr char kEmptyComponentLoaderPolicyMetricsSuffix[] =
    "WebViewEmptyComponent";

// A fake SHA256 PublicKey of jebgalgnebhfojomionfpkfelancnnkf
const uint8_t kFakePublicKeySHA256[32] = {
    0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec, 0x8e, 0xd5, 0xfa,
    0x54, 0xb0, 0xd2, 0xdd, 0xa5, 0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47,
    0xf6, 0xc4, 0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

}  // namespace

void EmptyComponentLoaderPolicy::ComponentLoaded(
    const base::Version& /*version*/,
    base::flat_map<std::string, base::ScopedFD>& /*fd_map*/,
    base::Value::Dict /*manifest*/) {}

void EmptyComponentLoaderPolicy::ComponentLoadFailed(
    component_updater::ComponentLoadResult /*error*/) {}

void EmptyComponentLoaderPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kFakePublicKeySHA256,
               kFakePublicKeySHA256 + std::size(kFakePublicKeySHA256));
}

std::string EmptyComponentLoaderPolicy::GetMetricsSuffix() const {
  return kEmptyComponentLoaderPolicyMetricsSuffix;
}

void LoadEmptyComponent(
    component_updater::ComponentLoaderPolicyVector& policies) {
  if (!base::FeatureList::IsEnabled(
          android_webview::features::kWebViewEmptyComponentLoaderPolicy)) {
    return;
  }

  policies.push_back(std::make_unique<EmptyComponentLoaderPolicy>());
}

}  // namespace android_webview
