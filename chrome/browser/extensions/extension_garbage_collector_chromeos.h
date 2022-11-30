// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_CHROMEOS_H_

#include "chrome/browser/extensions/extension_garbage_collector.h"

namespace extensions {

// Chrome OS specific extensions garbage collector. In addition to base class
// it also cleans up extensions install directory in shared location, see
// ExtensionAssetsManagerChromeOS.
class ExtensionGarbageCollectorChromeOS : public ExtensionGarbageCollector {
 public:
  explicit ExtensionGarbageCollectorChromeOS(content::BrowserContext* context);

  ExtensionGarbageCollectorChromeOS(const ExtensionGarbageCollectorChromeOS&) =
      delete;
  ExtensionGarbageCollectorChromeOS& operator=(
      const ExtensionGarbageCollectorChromeOS&) = delete;

  ~ExtensionGarbageCollectorChromeOS() override;

  static ExtensionGarbageCollectorChromeOS* Get(
      content::BrowserContext* context);

  // Enable or disable garbage collection. See |disable_garbage_collection_|.
  void disable_garbage_collection() { disable_garbage_collection_ = true; }
  void enable_garbage_collection() { disable_garbage_collection_ = false; }

  // Clear shared_extensions_garbage_collected_ to initiate more than one
  // GC in the same process for testing.
  static void ClearGarbageCollectedForTesting();

 private:
  // Overriddes for ExtensionGarbageCollector:
  void GarbageCollectExtensions() override;

  // Return true if there is no extension installation for all active profiles.
  bool CanGarbageCollectSharedExtensions();

  // Do GC for shared extensions dir.
  void GarbageCollectSharedExtensions();

  // TODO(rkc): HACK alert - this is only in place to allow the
  // kiosk_mode_screensaver to prevent its extension from getting garbage
  // collected. Remove this once KioskModeScreensaver is removed.
  // See crbug.com/280363
  bool disable_garbage_collection_;

  // Shared extensions need to be processed only once but instances of this
  // class are created per-profile so this static variable prevents multiple
  // processing.
  static bool shared_extensions_garbage_collected_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_CHROMEOS_H_
