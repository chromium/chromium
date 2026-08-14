// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/delayload_helpers.h"

#include <esent.h>

#include "base/test/gmock_expected_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

using base::test::ValueIs;

TEST(BaseWinDelayloadHelpersTest, IsDelayLoadFailureSuppressed) {
  EXPECT_FALSE(IsDelayLoadFailureSuppressed());
  {
    delayload_internal::ScopedSuppressDelayLoadFailure suppress;
    EXPECT_TRUE(IsDelayLoadFailureSuppressed());
  }
  EXPECT_FALSE(IsDelayLoadFailureSuppressed());
}

TEST(BaseWinDelayloadHelpersTest, LoadAllImportsForDll) {
  // Attempt to pre-load a delayloaded dll - use ESENT.dll.JetCloseDatabase as
  // nothing in base:: is likely to need to call that in future but ESENT.dll is
  // present on all Windows systems.
  EXPECT_TRUE(LoadAllImportsForDll("ESENT.dll"));

  // Expect this to fail, but not crash, as there is no database opened.
  JET_DBID dbid{};
  JET_ERR jet_error = JetCloseDatabase(NULL, dbid, 0);
  EXPECT_NE(jet_error, JET_errSuccess);

  // Expect that a module this module does not depend on does not load.
  EXPECT_FALSE(LoadAllImportsForDll("not-a-module.dll"));

  // Should be harmless to call this if a dll is not delayloaded.
  EXPECT_FALSE(LoadAllImportsForDll("VERSION.dll"));
}

TEST(BaseWinDelayloadHelpersTest, LoadAllImportsForDllUnchecked) {
  ASSERT_THAT(LoadAllImportsForDllUnchecked("ESENT.dll"), ValueIs(true));
  ASSERT_THAT(LoadAllImportsForDllUnchecked("not-a-module.dll"),
              ValueIs(false));
  ASSERT_THAT(LoadAllImportsForDllUnchecked("VERSION.dll"), ValueIs(false));
}

}  // namespace base::win
