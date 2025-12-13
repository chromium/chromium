// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_LIMITED_ACCESS_FEATURES_H_
#define BASE_WIN_LIMITED_ACCESS_FEATURES_H_

#include <string>

#include "base/base_export.h"

namespace base::win {

// Function to unlock a Windows Limited Access Feature.
// Limited Access Features are Windows platform features which require
// specific approval from Microsoft to be used in an application. Using them
// requires a specific feature ID `feature` and use token `token`.
BASE_EXPORT bool TryToUnlockLimitedAccessFeature(const std::wstring& feature,
                                                 const std::wstring& token);

}  // namespace base::win

#endif  // BASE_WIN_LIMITED_ACCESS_FEATURES_H_
