// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Proxy Settings API relevant classes to realize
// the API as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_H_

#include <string>

#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/extensions/pref_transformer_interface.h"
#include "components/proxy_config/proxy_prefs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {
class EventRouterForwarder;

// Class to convert between the representation of proxy settings used
// in the Proxy Settings API and the representation used in the PrefStores.
// This plugs into the ExtensionPreferenceAPI to get and set proxy settings.
class ProxyPrefTransformer : public PrefTransformerInterface {
 public:
  ProxyPrefTransformer();

  ProxyPrefTransformer(const ProxyPrefTransformer&) = delete;
  ProxyPrefTransformer& operator=(const ProxyPrefTransformer&) = delete;

  ~ProxyPrefTransformer() override;

  // Implementation of PrefTransformerInterface.
  absl::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) override;
  absl::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) override;
};

// This class observes proxy error events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ProxyEventRouter {
 public:
  ProxyEventRouter(const ProxyEventRouter&) = delete;
  ProxyEventRouter& operator=(const ProxyEventRouter&) = delete;

  static ProxyEventRouter* GetInstance();

  void OnProxyError(EventRouterForwarder* event_router,
                    void* profile,
                    int error_code);

  void OnPACScriptError(EventRouterForwarder* event_router,
                        void* profile,
                        int line_number,
                        const std::u16string& error);

 private:
  friend struct base::DefaultSingletonTraits<ProxyEventRouter>;

  ProxyEventRouter();
  ~ProxyEventRouter();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_H_
