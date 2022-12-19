// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_winrt_initializer.h"

#include "base/test/gtest_util.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

TEST(ScopedWinrtInitializer, BasicFunctionality) {
  AssertComApartmentType(ComApartmentType::NONE);
  {
    ScopedWinrtInitializer scoped_winrt_initializer;
    AssertComApartmentType(ComApartmentType::MTA);
  }
  AssertComApartmentType(ComApartmentType::NONE);
}

TEST(ScopedWinrtInitializer, ApartmentChangeCheck) {
  ScopedCOMInitializer com_initializer;
  // ScopedCOMInitializer initialized an STA and the following should be a
  // failed request for an MTA.
  EXPECT_DCHECK_DEATH({ ScopedWinrtInitializer scoped_winrt_initializer; });
}

}  // namespace base::win
