// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_LLVM_PROFILE_UTIL_H_
#define CHROME_APP_LLVM_PROFILE_UTIL_H_

#include <string>
#include <string_view>

// Represents the process type for target-specific profile collection.
enum class ProfileProcessType {
  kRenderer,
};

// Returns the profile filename with the appropriate prefix for `process_type`
// if `current` starts with "default-". Returns empty string otherwise.
std::string GetLLVMProfileFilename(std::string_view current,
                                   ProfileProcessType process_type);

// Replaces the "default-" prefix in the LLVM profile filename template with
// the appropriate prefix for the given `process_type`. For example,
// "path/to/default-%p-%m.profraw" becomes "path/to/renderer-%p-%m.profraw" for
// `ProfileProcessType::kRenderer`.
void SetLLVMProfileProcessType(ProfileProcessType process_type);

#endif  // CHROME_APP_LLVM_PROFILE_UTIL_H_
