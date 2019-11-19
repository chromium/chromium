// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SPECIAL_STORAGE_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SPECIAL_STORAGE_POLICY_H_

#include <map>
#include <memory>
#include <string>

#include "base/synchronization/lock.h"
#include "extensions/common/extension_set.h"
#include "services/network/session_cleanup_cookie_store.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"

namespace content_settings {
class CookieSettings;
}

namespace extensions {
class Extension;
}

// Special rights are granted to 'extensions' and 'applications'. The
// storage subsystems and the browsing data remover query this interface
// to determine which origins have these rights.
class ExtensionSpecialStoragePolicy : public storage::SpecialStoragePolicy {
 public:
  explicit ExtensionSpecialStoragePolicy(
      content_settings::CookieSettings* cookie_settings);

  // storage::SpecialStoragePolicy methods used by storage subsystems and the
  // browsing data remover. These methods are safe to call on any thread.
  bool IsStorageProtected(const GURL& origin) override;
  bool IsStorageUnlimited(const GURL& origin) override;
  bool IsStorageSessionOnly(const GURL& origin) override;
  bool HasIsolatedStorage(const GURL& origin) override;
  bool HasSessionOnlyOrigins() override;
  bool IsStorageDurable(const GURL& origin) override;
  network::SessionCleanupCookieStore::DeleteCookiePredicate
  CreateDeleteCookieOnExitPredicate() override;

  // Methods used by the ExtensionService to populate this class.
  void GrantRightsForExtension(const extensions::Extension* extension);
  void RevokeRightsForExtension(const extensions::Extension* extension);
  void RevokeRightsForAllExtensions();

  // Decides whether the storage for |extension|'s web extent needs protection.
  bool NeedsProtection(const extensions::Extension* extension);

  // Returns the set of extensions protecting this origin. The caller does not
  // take ownership of the return value.
  const extensions::ExtensionSet* ExtensionsProtectingOrigin(
      const GURL& origin);

 protected:
  ~ExtensionSpecialStoragePolicy() override;

 private:
  class SpecialCollection {
   public:
    SpecialCollection();
    ~SpecialCollection();

    bool Contains(const GURL& origin);
    bool GrantsCapabilitiesTo(const GURL& origin);
    const extensions::ExtensionSet* ExtensionsContaining(const GURL& origin);
    bool ContainsExtension(const std::string& extension_id);
    bool Add(const extensions::Extension* extension);
    bool Remove(const extensions::Extension* extension);
    void Clear();

   private:
    void ClearCache();

    extensions::ExtensionSet extensions_;
    std::map<GURL, std::unique_ptr<extensions::ExtensionSet>> cached_results_;
  };

  void NotifyGranted(const GURL& origin, int change_flags);
  void NotifyRevoked(const GURL& origin, int change_flags);
  void NotifyCleared();

  base::Lock lock_;  // Synchronize all access to the collections.
  SpecialCollection protected_apps_;
  SpecialCollection unlimited_extensions_;
  SpecialCollection file_handler_extensions_;
  SpecialCollection isolated_extensions_;
  SpecialCollection content_capabilities_unlimited_extensions_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SPECIAL_STORAGE_POLICY_H_
