// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_COMPONENT_EXTENSIONS_ALLOWLIST_ALLOWLIST_H_
#define CHROME_BROWSER_EXTENSIONS_COMPONENT_EXTENSIONS_ALLOWLIST_ALLOWLIST_H_

#include <string>

#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// =============================================================================
//
// ADDING NEW EXTENSIONS REQUIRES APPROVAL from Extensions Tech Lead:
// rdevlin.cronin@chromium.org
//
// For more information on component extensions, see
// //extensions/docs/component_extensions.md.
//
// =============================================================================

// Checks using an extension ID.
bool IsComponentExtensionAllowlisted(const std::string& extension_id);

// Checks using resource ID of manifest.
bool IsComponentExtensionAllowlisted(int manifest_resource_id);

#if BUILDFLAG(IS_CHROMEOS)
// Checks using extension id for sign in profile.
bool IsComponentExtensionAllowlistedForSignInProfile(
    const std::string& extension_id);
#endif

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_COMPONENT_EXTENSIONS_ALLOWLIST_ALLOWLIST_H_
