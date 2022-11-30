// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_REGISTRY_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_REGISTRY_H_

#include <map>

#include "chrome/browser/ash/system_extensions/system_extension.h"

namespace ash {

struct SystemExtension;

// SystemExtensionsRegistry holds the set of the installed system extensions.
// Exposes methods clients can use to get SystemExtensions.
class SystemExtensionsRegistry {
 public:
  // Returns the ids of all installed System Extensions.
  virtual std::vector<SystemExtensionId> GetIds() = 0;

  // Returns the SystemExtension with `system_extension_id`.
  virtual const SystemExtension* GetById(
      const SystemExtensionId& system_extension_id) = 0;

  // Returns the SystemExtension that has the same origin as `url`.
  virtual const SystemExtension* GetByUrl(const GURL& url) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_REGISTRY_H_
