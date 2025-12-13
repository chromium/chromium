// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MV2_DEPRECATION_IMPACT_CHECKER_H_
#define CHROME_BROWSER_EXTENSIONS_MV2_DEPRECATION_IMPACT_CHECKER_H_

#include "base/memory/raw_ptr.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
class Extension;
class ExtensionManagement;
class HashedExtensionId;

// A helper class to determine if an extension is affected by the MV2
// deprecation experiments.
// NOTE: Instead of using this class directly, callers should go through the
// ManifestV2ExperimentManager.
class MV2DeprecationImpactChecker {
 public:
  explicit MV2DeprecationImpactChecker(
      ExtensionManagement* extension_management);
  ~MV2DeprecationImpactChecker();

  // Returns true if the given `extension` is affected by the MV2 deprecation.
  // This may be false if, e.g., the extension is policy-installed.
  bool IsExtensionAffected(const Extension& extension);
  // Same as above, but allows for passing in the relevant bits from the
  // extension directly in case the `Extension` object doesn't yet exist.
  bool IsExtensionAffected(const ExtensionId& extension_id,
                           int manifest_version,
                           Manifest::Type manifest_type,
                           mojom::ManifestLocation manifest_location,
                           const HashedExtensionId& hashed_id);

 private:
  // The associated `ExtensionManagement` class. Must be guaranteed to outlive
  // this class.
  raw_ptr<ExtensionManagement> extension_management_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MV2_DEPRECATION_IMPACT_CHECKER_H_
