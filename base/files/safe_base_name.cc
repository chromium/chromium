// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/safe_base_name.h"

namespace base {

// static
absl::optional<SafeBaseName> SafeBaseName::Create(const FilePath& path) {
  auto basename = path.BaseName();

  if (!basename.IsAbsolute() && !basename.ReferencesParent() &&
      !basename.EndsWithSeparator()) {
    return absl::make_optional(SafeBaseName(basename));
  }

  return absl::nullopt;
}

// static
absl::optional<SafeBaseName> SafeBaseName::Create(
    FilePath::StringPieceType path) {
  return Create(FilePath(path));
}

SafeBaseName::SafeBaseName(const FilePath& path) : path_(path) {}

bool SafeBaseName::operator==(const SafeBaseName& that) const {
  return path_ == that.path_;
}

}  // namespace base
