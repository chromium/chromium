// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FLAGS_CHROME_CACHED_FLAGS_H_
#define CHROME_BROWSER_ANDROID_FLAGS_CHROME_CACHED_FLAGS_H_

#include <optional>

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"

namespace chrome::android {

bool IsJavaDrivenFeatureEnabled(const base::Feature& feature);

// Overrides a Java-driven feature (`CachedFlag` / `FeatureOverrides`) for the
// lifetime of this object, restoring its prior Java test override state on
// destruction without disturbing other Java feature overrides.
class ScopedJavaDrivenFeatureForTesting {
 public:
  ScopedJavaDrivenFeatureForTesting(const base::Feature& feature, bool enabled);
  ScopedJavaDrivenFeatureForTesting(const ScopedJavaDrivenFeatureForTesting&) =
      delete;
  ScopedJavaDrivenFeatureForTesting& operator=(
      const ScopedJavaDrivenFeatureForTesting&) = delete;
  ~ScopedJavaDrivenFeatureForTesting();

 private:
  const base::raw_ref<const base::Feature> feature_;
  std::optional<bool> previous_override_;
};

}  // namespace chrome::android

#endif  // CHROME_BROWSER_ANDROID_FLAGS_CHROME_CACHED_FLAGS_H_
