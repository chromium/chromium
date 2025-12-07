// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_KEYED_SERVICES_CHROME_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
#define CHROME_BROWSER_EXTENSIONS_KEYED_SERVICES_CHROME_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace chrome_extensions {

// Ensures the existence of any BrowserContextKeyedServiceFactory provided by
// the Chrome extensions code.
void EnsureChromeBrowserContextKeyedServiceFactoriesBuilt();

}  // namespace chrome_extensions

#endif  // CHROME_BROWSER_EXTENSIONS_KEYED_SERVICES_CHROME_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
