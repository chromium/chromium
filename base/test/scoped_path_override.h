// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_PATH_OVERRIDE_H_
#define BASE_TEST_SCOPED_PATH_OVERRIDE_H_

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

class FilePath;

// Sets a path override on construction, and removes it when the object goes out
// of scope. This class is intended to be used by tests that need to override
// paths to ensure their overrides are properly handled and reverted when the
// scope of the test is left.
class ScopedPathOverride {
 public:
  // Contructor that initializes the override to a scoped temp directory.
  explicit ScopedPathOverride(int key);

  // Constructor that would use a path provided by the user.
  ScopedPathOverride(int key, const FilePath& dir);

  // See PathService::OverrideAndCreateIfNeeded.
  ScopedPathOverride(int key,
                     const FilePath& path,
                     bool is_absolute,
                     bool create);
  ~ScopedPathOverride();

 private:
  // Used for saving original_override_ when an override already exists.
  void SaveOriginal();

  int key_;
  ScopedTempDir temp_dir_;
  absl::optional<FilePath> original_override_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPathOverride);
};

}  // namespace base

#endif  // BASE_TEST_SCOPED_PATH_OVERRIDE_H_
