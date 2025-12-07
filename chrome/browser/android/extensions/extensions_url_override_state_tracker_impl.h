// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_STATE_TRACKER_IMPL_H_
#define CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_STATE_TRACKER_IMPL_H_

#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/android/extensions/extensions_url_override_registry_manager.h"
#include "chrome/browser/android/extensions/extensions_url_override_state_tracker.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/chrome_url_overrides_handler.h"
#include "url/gurl.h"

namespace extensions {

// Provides extensions URL overrides state tracking to a
// ExtensionsUrlOverrideRegistryManager.
class ExtensionUrlOverrideStateTrackerImpl
    : public ExtensionUrlOverrideStateTracker {
 public:
  using IncognitoStatusToOverrideCount = base::flat_map<bool, int>;

  ExtensionUrlOverrideStateTrackerImpl(Profile* profile,
                                       StateListener* listener);
  ~ExtensionUrlOverrideStateTrackerImpl();

  ExtensionUrlOverrideStateTrackerImpl(
      const ExtensionUrlOverrideStateTrackerImpl& client) = delete;
  ExtensionUrlOverrideStateTrackerImpl& operator=(
      const ExtensionUrlOverrideStateTrackerImpl& client) = delete;

 private:
  // Synchronizes the state tracker to the Extensions Override Registrar.
  class RegistrarSynchronizer
      : public ExtensionWebUIOverrideRegistrar::Observer {
   public:
    RegistrarSynchronizer(Profile* profile,
                          ExtensionUrlOverrideStateTrackerImpl* state_tracker);

    // Called when an extension with a URL override is added and enabled.
    void OnExtensionOverrideAdded(const Extension& extension) override;

    // Called when an extension with a URL override is removed or disabled.
    void OnExtensionOverrideRemoved(const Extension& extension) override;

   private:
    raw_ptr<ExtensionUrlOverrideStateTrackerImpl> state_tracker_;
  };

  // To be called when an individual extension's override for a URL is
  // registered or activated.
  void OnUrlOverrideRegistered(const Extension& extension,
                               const URLOverrides::URLOverrideMap& overrides);

  // To be called when an individual extension's override for a URL is
  // deactivated.
  void OnUrlOverrideDeactivated(const Extension& extension,
                                const URLOverrides::URLOverrideMap& overrides);

  // Updates the override counts for the given `overrides` by `overrides_delta`.
  // `overrides_delta` is 1 for registration / activation and -1 for
  // deactivation. Notifies observers if the state of an override changes.
  void UpdateOverrides(const Extension& extension,
                       const URLOverrides::URLOverrideMap& overrides,
                       int overrides_delta);

  void EnsureOverridesInitialized(
      const URLOverrides::URLOverrideMap& overrides);
  bool GetAndCacheIncognitoStatus(const Extension& extension);

  base::flat_map<std::string, bool> extension_id_to_incognito_status_;
  base::flat_map<std::string, IncognitoStatusToOverrideCount> override_map_;

  std::unique_ptr<RegistrarSynchronizer> synchronizer_;

  raw_ptr<ExtensionWebUIOverrideRegistrar> registrar_;
  raw_ptr<StateListener> listener_;
  raw_ptr<Profile> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ANDROID_EXTENSIONS_EXTENSIONS_URL_OVERRIDE_STATE_TRACKER_IMPL_H_
