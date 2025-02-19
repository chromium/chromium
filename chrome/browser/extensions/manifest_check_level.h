// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MANIFEST_CHECK_LEVEL_H_
#define CHROME_BROWSER_EXTENSIONS_MANIFEST_CHECK_LEVEL_H_

namespace extensions {

// The amount of manifest checking to perform.
enum class ManifestCheckLevel {
  // Do not check for any manifest equality.
  kNone,

  // Only check that the expected and actual permissions have the same
  // effective permissions.
  kLoose,

  // All data in the expected and actual manifests must match.
  kStrict,
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MANIFEST_CHECK_LEVEL_H_
