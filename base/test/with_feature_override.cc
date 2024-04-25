// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/with_feature_override.h"
#include "base/task/thread_pool/thread_pool_instance.h"

namespace base {
namespace test {

WithFeatureOverride::WithFeatureOverride(const base::Feature& feature) {
  // Most other classes that tests inherit from start task environments. Verify
  // that has not happened yet.
  DCHECK(base::ThreadPoolInstance::Get() == nullptr)
      << "WithFeatureOverride should be the first class a test inherits from "
         "so it sets the features before any other setup is done.";

  if (GetParam()) {
    scoped_feature_list_.InitAndEnableFeature(feature);
  } else {
    scoped_feature_list_.InitAndDisableFeature(feature);
  }
}

bool WithFeatureOverride::IsParamFeatureEnabled() const {
  return GetParam();
}

WithFeatureOverride::~WithFeatureOverride() = default;

}  // namespace test
}  // namespace base
