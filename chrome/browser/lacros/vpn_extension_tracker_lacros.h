// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_VPN_EXTENSION_TRACKER_LACROS_H_
#define CHROME_BROWSER_LACROS_VPN_EXTENSION_TRACKER_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

// Provides a way for Ash to be aware of Lacros Vpn extension events.
class VpnExtensionTrackerLacros : public extensions::ExtensionRegistryObserver {
 public:
  VpnExtensionTrackerLacros();
  ~VpnExtensionTrackerLacros() override;

  // Starts observing the ExtensionRegistry.
  void Start();

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext*,
                         const extensions::Extension*) override;
  void OnExtensionUnloaded(content::BrowserContext*,
                           const extensions::Extension*,
                           extensions::UnloadedExtensionReason) override;
  void OnShutdown(extensions::ExtensionRegistry*) override;

 private:
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::WeakPtrFactory<VpnExtensionTrackerLacros> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_VPN_EXTENSION_TRACKER_LACROS_H_
