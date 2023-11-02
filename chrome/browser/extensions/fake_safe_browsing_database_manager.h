// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_

#include <set>
#include <string>

#include "components/safe_browsing/core/browser/db/test_database_manager.h"

namespace extensions {

// A fake safe browsing database manager for use with extensions tests.
//
// By default it is disabled (returning true and ignoring |unsafe_ids_|);
// call set_enabled to enable it.
class FakeSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  explicit FakeSafeBrowsingDatabaseManager(bool enabled);

  // Returns true if synchronously safe, false if not in which case the unsafe
  // IDs taken from |unsafe_ids_| are passed to to |client| on the current
  // message loop.
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;

  // Return |this| to chain together SetUnsafe(...).NotifyUpdate() conveniently.
  FakeSafeBrowsingDatabaseManager& Enable();
  FakeSafeBrowsingDatabaseManager& Disable();
  FakeSafeBrowsingDatabaseManager& ClearUnsafe();
  FakeSafeBrowsingDatabaseManager& SetUnsafe(const std::string& a);
  FakeSafeBrowsingDatabaseManager& SetUnsafe(const std::string& a,
                                             const std::string& b);
  FakeSafeBrowsingDatabaseManager& SetUnsafe(const std::string& a,
                                             const std::string& b,
                                             const std::string& c);
  FakeSafeBrowsingDatabaseManager& SetUnsafe(const std::string& a,
                                             const std::string& b,
                                             const std::string& c,
                                             const std::string& d);
  FakeSafeBrowsingDatabaseManager& AddUnsafe(const std::string& a);
  FakeSafeBrowsingDatabaseManager& RemoveUnsafe(const std::string& a);

  // Send the update notification.
  void NotifyUpdate();

 private:
  ~FakeSafeBrowsingDatabaseManager() override;

  // Whether to respond to CheckExtensionIDs immediately with true (indicating
  // that there is definitely no extension ID match).
  bool enabled_;

  // The extension IDs considered unsafe.
  std::set<std::string> unsafe_ids_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_
