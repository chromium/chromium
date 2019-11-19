// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

const char kCroshExtensionEntryPoint[] = "/html/crosh.html";

}  // namespace

const Extension* TerminalExtensionHelper::GetTerminalExtension(
    Profile* profile) {
  // Search order for terminal extensions.
  // We prefer hterm-dev, then hterm, then the builtin crosh extension.
  static const char* const kPossibleAppIds[] = {
    extension_misc::kHTermDevAppId,
    extension_misc::kHTermAppId,
    extension_misc::kCroshBuiltinAppId,
  };

  // The hterm-dev should be first in the list.
  DCHECK_EQ(kPossibleAppIds[0], extension_misc::kHTermDevAppId);

  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile)->enabled_extensions();
  for (size_t i = 0; i < base::size(kPossibleAppIds); ++i) {
    const extensions::Extension* extension =
        extensions.GetByID(kPossibleAppIds[i]);
    if (extension)
      return extension;
  }

  return nullptr;
}

GURL TerminalExtensionHelper::GetCroshExtensionURL(Profile* profile) {
  GURL url;
  const extensions::Extension* extension = GetTerminalExtension(profile);
  if (extension)
    url = extension->GetResourceURL(kCroshExtensionEntryPoint);
  return url;
}

}  // namespace extensions
