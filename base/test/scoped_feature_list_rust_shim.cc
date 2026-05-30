// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list_rust_shim.h"

#include "base/feature_list.h"

namespace base::test::rust {

ScopedFeatureListRs::ScopedFeatureListRs() = default;
ScopedFeatureListRs::~ScopedFeatureListRs() = default;

void ScopedFeatureListRs::InitFromCommandLine(::rust::Str enable_features,
                                              ::rust::Str disable_features) {
  scoped_feature_list_.InitFromCommandLine(std::string_view(enable_features),
                                           std::string_view(disable_features));
}

void ScopedFeatureListRs::InitAndEnableFeature(const base::Feature& feature) {
  scoped_feature_list_.InitAndEnableFeature(feature);
}

void ScopedFeatureListRs::InitAndDisableFeature(const base::Feature& feature) {
  scoped_feature_list_.InitAndDisableFeature(feature);
}

// On the C++ side, ScopedFeatureList is managed via RAII.
// We can't rely on that on the Rust side. Instead, we make a UniquePtr
// and use that to interface with it.
std::unique_ptr<ScopedFeatureListRs> CreateScopedFeatureListRs() {
  return std::make_unique<ScopedFeatureListRs>();
}

}  // namespace base::test::rust
