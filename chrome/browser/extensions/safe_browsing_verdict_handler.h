// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SAFE_BROWSING_VERDICT_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_SAFE_BROWSING_VERDICT_HANDLER_H_

#include <string>

#include "chrome/browser/extensions/blocklist.h"
#include "extensions/common/extension_set.h"

namespace extensions {
class ExtensionPrefs;
class ExtensionRegistry;
class ExtensionService;

// Manages the Safe Browsing blocklist/greylist state in extension pref.
class SafeBrowsingVerdictHandler {
 public:
  SafeBrowsingVerdictHandler(ExtensionPrefs* extension_prefs,
                             ExtensionRegistry* registry,
                             ExtensionService* extension_service);
  SafeBrowsingVerdictHandler(const SafeBrowsingVerdictHandler&) = delete;
  SafeBrowsingVerdictHandler& operator=(const SafeBrowsingVerdictHandler&) =
      delete;
  ~SafeBrowsingVerdictHandler() = default;

  // Partitions `before`, `after` and `unchanged` into `no_longer` and
  // `not_yet`. `no_longer` = `before` - `after` - `unchanged`. `not_yet` =
  // `after` - `before`.
  static void Partition(const ExtensionIdSet& before,
                        const ExtensionIdSet& after,
                        const ExtensionIdSet& unchanged,
                        ExtensionIdSet* no_longer,
                        ExtensionIdSet* not_yet);

  // Initializes and load greylist from prefs.
  void Init();

  // Manages the blocklisted extensions. Enables/disables/loads/unloads
  // extensions based on the current `state_map`.
  // TODO(crbug.com/1193695): This function currently only handles greylist
  // states. We should move blocklist handling into this class too.
  void ManageBlocklist(const Blocklist::BlocklistStateMap& state_map);

 private:
  // Adds extensions in `greylist` to `greylist_` and disables them. Removes
  // extensions that are neither in `greylist`, nor in `unchanged` from
  // `greylist_` and maybe re-enable them.
  void UpdateGreylistedExtensions(
      const ExtensionIdSet& greylist,
      const ExtensionIdSet& unchanged,
      const Blocklist::BlocklistStateMap& state_map);

  ExtensionPrefs* extension_prefs_ = nullptr;
  ExtensionRegistry* registry_ = nullptr;
  ExtensionService* extension_service_ = nullptr;

  // Set of greylisted extensions. These extensions are disabled if they are
  // already installed in Chromium at the time when they are added to
  // the greylist. Unlike blocklisted extensions, greylisted ones are visible
  // to the user and if user re-enables such an extension, they remain enabled.
  //
  // These extensions should appear in registry_.
  ExtensionSet greylist_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SAFE_BROWSING_VERDICT_HANDLER_H_
