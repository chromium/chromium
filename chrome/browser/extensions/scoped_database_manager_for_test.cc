// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/scoped_database_manager_for_test.h"

#include "chrome/browser/extensions/blocklist.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"

using safe_browsing::SafeBrowsingDatabaseManager;

namespace extensions {

ScopedDatabaseManagerForTest::ScopedDatabaseManagerForTest(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager)
    : original_(Blocklist::GetDatabaseManager()) {
  Blocklist::SetDatabaseManager(database_manager);
}

ScopedDatabaseManagerForTest::~ScopedDatabaseManagerForTest() {
  Blocklist::SetDatabaseManager(original_);
}

}  // namespace extensions
