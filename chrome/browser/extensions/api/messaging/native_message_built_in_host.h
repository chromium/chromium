// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_BUILT_IN_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_BUILT_IN_HOST_H_

#include <stddef.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class NativeMessageHost;

struct NativeMessageBuiltInHost {
  // Unique name to identify the built-in host.
  const char* const name;

  // The extension origins allowed to create the built-in host.
  // This field is a raw_span<> because it only ever points at statically-
  // allocated memory which is never freed, and hence cannot dangle.
  const base::raw_span<const char* const> allowed_origins;

  // The factory function used to create new instances of this host.
  std::unique_ptr<NativeMessageHost> (*const create_function)(
      content::BrowserContext*);
};

// The set of built-in hosts that can be instantiated. These are defined in the
// platform-specific impl files.
extern const base::span<const NativeMessageBuiltInHost> kBuiltInHosts;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_BUILT_IN_HOST_H_
