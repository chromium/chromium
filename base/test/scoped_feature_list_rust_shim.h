// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_FEATURE_LIST_RUST_SHIM_H_
#define BASE_TEST_SCOPED_FEATURE_LIST_RUST_SHIM_H_

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace base::test::rust {

// ScopedFeatureListRs provides a simple C++/Rust FFI interface for
// base::test::ScopedFeatureList.
class ScopedFeatureListRs {
 public:
  ScopedFeatureListRs();
  ~ScopedFeatureListRs();

  void InitFromCommandLine(::rust::Str enable_features,
                           ::rust::Str disable_features);
  void InitAndEnableFeature(const base::Feature& feature);
  void InitAndDisableFeature(const base::Feature& feature);

 private:
  ScopedFeatureList scoped_feature_list_;
};

std::unique_ptr<ScopedFeatureListRs> CreateScopedFeatureListRs();

}  // namespace base::test::rust

#endif  // BASE_TEST_SCOPED_FEATURE_LIST_RUST_SHIM_H_
