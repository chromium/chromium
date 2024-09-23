// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Proxy Settings API relevant classes to realize
// the API as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_H_

#include <optional>
#include <string>

#include "base/memory/singleton.h"

namespace extensions {

// This class observes proxy error events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ProxyEventRouter {
 public:
  ProxyEventRouter(const ProxyEventRouter&) = delete;
  ProxyEventRouter& operator=(const ProxyEventRouter&) = delete;

  static ProxyEventRouter* GetInstance();

  void OnProxyError(void* browser_context, int error_code);

  void OnPACScriptError(void* browser_context,
                        int line_number,
                        const std::u16string& error);

 private:
  friend struct base::DefaultSingletonTraits<ProxyEventRouter>;

  ProxyEventRouter();
  ~ProxyEventRouter();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_H_
