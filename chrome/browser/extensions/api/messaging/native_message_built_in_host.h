// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_BUILT_IN_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_BUILT_IN_HOST_H_

#include <memory>
#include "base/memory/raw_ptr_exclusion.h"

#include <stddef.h>

namespace content {
class BrowserContext;
}

namespace extensions {

class NativeMessageHost;

struct NativeMessageBuiltInHost {
  // Unique name to identify the built-in host.
  const char* const name;

  // The extension origins allowed to create the built-in host.
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #global-scope
  RAW_PTR_EXCLUSION const char* const* const allowed_origins;

  // The count of |allowed_origins|.
  size_t allowed_origins_count;

  // The factory function used to create new instances of this host.
  std::unique_ptr<NativeMessageHost> (*create_function)(
      content::BrowserContext*);
};

// The set of built-in hosts that can be instantiated. These are defined in the
// platform-specific impl files.
extern const NativeMessageBuiltInHost kBuiltInHosts[];

// The count of built-in hosts defined in |kBuiltInHosts|.
extern const size_t kBuiltInHostsCount;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_BUILT_IN_HOST_H_
