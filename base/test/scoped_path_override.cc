// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_path_override.h"

#include <ostream>

#include "base/check.h"
#include "base/path_service.h"

namespace base {

ScopedPathOverride::ScopedPathOverride(int key) : key_(key) {
  SaveOriginal();
  bool result = temp_dir_.CreateUniqueTempDir();
  CHECK(result);
  result = PathService::Override(key, temp_dir_.GetPath());
  CHECK(result);
}

ScopedPathOverride::ScopedPathOverride(int key, const base::FilePath& dir)
    : key_(key) {
  SaveOriginal();
  bool result = PathService::Override(key, dir);
  CHECK(result);
}

ScopedPathOverride::ScopedPathOverride(int key,
                                       const FilePath& path,
                                       bool is_absolute,
                                       bool create)
    : key_(key) {
  SaveOriginal();
  bool result =
      PathService::OverrideAndCreateIfNeeded(key, path, is_absolute, create);
  CHECK(result);
}

void ScopedPathOverride::SaveOriginal() {
  if (PathService::IsOverriddenForTesting(key_)) {
    original_override_ = PathService::CheckedGet(key_);
  }
}

ScopedPathOverride::~ScopedPathOverride() {
  bool result = PathService::RemoveOverrideForTests(key_);
  CHECK(result) << "The override seems to have been removed already!";
  if (original_override_) {
    // PathService::Override, by default, does some (blocking) checks to ensure
    // that the path is absolute and exists. As the original override must have
    // already gone through these checks, we can skip these checks here.
    // This is needed for some tests which use ScopedPathOverride in scopes that
    // disallow blocking.
    result = PathService::OverrideAndCreateIfNeeded(
        key_, *original_override_, /*is_absolute=*/true, /*create=*/false);
    CHECK(result);
  }
}

}  // namespace base
