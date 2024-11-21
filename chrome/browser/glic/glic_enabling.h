// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_ENABLING_H_
#define CHROME_BROWSER_GLIC_GLIC_ENABLING_H_

#include "base/types/expected.h"

namespace glic {
// Enum for signalling the reason why glic was not enabled.
enum class GlicEnabledStatus {
  kEnabled = 0,
  kGlicFeatureFlagDisabled = 1,
  kTabstripComboButtonDisabled = 2,
  kMaxValue = 2
};
}  // namespace glic

// This class provides a central location for checking if GLIC is enabled. It
// allows for future expansion to include other ways the feature may be disabled
// such as based on user preferences or system settings.
class GlicEnabling {
 public:
  // Returns whether the Glic feature is enabled for Chrome. This status will
  // not change at runtime.
  static bool IsEnabledByFlags();

 private:
  // Private helper function that returns enabled status for fine grain logging
  // if desired.
  static glic::GlicEnabledStatus CheckEnabling();
};

#endif  // CHROME_BROWSER_GLIC_GLIC_ENABLING_H_
