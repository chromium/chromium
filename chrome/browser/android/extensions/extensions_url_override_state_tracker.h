// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_STATE_TRACKER_H_
#define CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_STATE_TRACKER_H_

#include "chrome/browser/profiles/profile.h"

namespace extensions {

// Provides extensions URL overrides state tracking to a
// ExtensionsUrlOverrideRegistryManager.
class ExtensionUrlOverrideStateTracker {
 public:
  class StateListener {
   public:
    // To be called when overrides now exist for this chrome URL path.
    // `incognito_enabled` refers to whether this URL Override is enabled for
    // use in incognito. This can also be called again when the
    // incognito_enabled status of this override changes.
    virtual void OnUrlOverrideEnabled(const std::string& chrome_url_path,
                                      bool incognito_enabled) = 0;

    // To be called when overrides no longer exist for this chrome URL path.
    virtual void OnUrlOverrideDisabled(const std::string& chrome_url_path) = 0;
  };

  ExtensionUrlOverrideStateTracker() = default;
  ~ExtensionUrlOverrideStateTracker() = default;

  ExtensionUrlOverrideStateTracker(
      const ExtensionUrlOverrideStateTracker& client) = delete;
  ExtensionUrlOverrideStateTracker& operator=(
      const ExtensionUrlOverrideStateTracker& client) = delete;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_STATE_TRACKER_H_
