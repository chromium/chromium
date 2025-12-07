// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/delayload_helpers.h"

#include <esent.h>

#include "base/test/gmock_expected_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

TEST(BaseWinDelayloadHelpersTest, LoadAllImportsForDll) {
  bool loaded;
  // Attempt to pre-load a delayloaded dll - use ESENT.dll.JetCloseDatabase as
  // nothing in base:: is likely to need to call that in future but ESENT.dll is
  // present on all Windows systems.
  ASSERT_OK_AND_ASSIGN(loaded, LoadAllImportsForDll("ESENT.dll"));
  EXPECT_TRUE(loaded);

  // Expect this to fail, but not crash, as there is no database opened.
  JET_DBID dbid{};
  JET_ERR jet_error = JetCloseDatabase(NULL, dbid, 0);
  EXPECT_NE(jet_error, JET_errSuccess);

  // Expect that a module this module does not depend on does not load.
  ASSERT_OK_AND_ASSIGN(loaded, LoadAllImportsForDll("not-a-module.dll"));
  EXPECT_FALSE(loaded);

  // Should be harmless to call this if a dll is not delayloaded.
  ASSERT_OK_AND_ASSIGN(loaded, LoadAllImportsForDll("VERSION.dll"));
  EXPECT_FALSE(loaded);
}

}  // namespace base::win
