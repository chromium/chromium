// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCOPED_DATABASE_MANAGER_FOR_TEST_H_
#define CHROME_BROWSER_EXTENSIONS_SCOPED_DATABASE_MANAGER_FOR_TEST_H_

#include "base/memory/scoped_refptr.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"

namespace extensions {

class ScopedDatabaseManagerForTest {
 public:
  explicit ScopedDatabaseManagerForTest(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager);

  ScopedDatabaseManagerForTest(const ScopedDatabaseManagerForTest&) = delete;
  ScopedDatabaseManagerForTest& operator=(const ScopedDatabaseManagerForTest&) =
      delete;

  ~ScopedDatabaseManagerForTest();

 private:
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> original_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCOPED_DATABASE_MANAGER_FOR_TEST_H_
