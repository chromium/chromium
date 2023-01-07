// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_NET_LACROS_EXTENSION_PROXY_TRACKER_H_
#define CHROME_BROWSER_LACROS_NET_LACROS_EXTENSION_PROXY_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace lacros {
namespace net {

// Tracks proxies set by extensions in the Lacros primary profile and forwards
// the proxy configuration to Ash-Chrome via mojo.
class LacrosExtensionProxyTracker {
 public:
  explicit LacrosExtensionProxyTracker(Profile* profile_);
  LacrosExtensionProxyTracker(const LacrosExtensionProxyTracker&) = delete;
  LacrosExtensionProxyTracker& operator=(const LacrosExtensionProxyTracker&) =
      delete;
  ~LacrosExtensionProxyTracker();

  static bool AshVersionSupportsExtensionSetProxies();

 private:
  void OnProxyPrefChanged(const std::string& pref_name);

  bool extension_proxy_active_ = false;
  raw_ptr<Profile> profile_;
  PrefChangeRegistrar proxy_prefs_registrar_;
};
}  // namespace net
}  // namespace lacros

#endif  // CHROME_BROWSER_LACROS_NET_LACROS_EXTENSION_PROXY_TRACKER_H_
