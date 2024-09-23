// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/com_init_util.h"

#include "base/test/gtest_util.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

TEST(ComInitUtil, AssertNotInitialized) {
  if (DCHECK_IS_ON()) {
    EXPECT_NOTREACHED_DEATH(AssertComInitialized());
  }
}

TEST(ComInitUtil, AssertUninitialized) {
  // When COM is uninitialized, the TLS data will remain, but the apartment
  // status will be updated. This covers that case.
  {
    ScopedCOMInitializer com_initializer;
    ASSERT_TRUE(com_initializer.Succeeded());
  }
  if (DCHECK_IS_ON()) {
    EXPECT_NOTREACHED_DEATH(AssertComInitialized());
  }
}

TEST(ComInitUtil, AssertSTAInitialized) {
  ScopedCOMInitializer com_initializer;
  ASSERT_TRUE(com_initializer.Succeeded());

  AssertComInitialized();
}

TEST(ComInitUtil, AssertMTAInitialized) {
  ScopedCOMInitializer com_initializer(ScopedCOMInitializer::kMTA);
  ASSERT_TRUE(com_initializer.Succeeded());

  AssertComInitialized();
}

TEST(ComInitUtil, AssertNoneApartmentType) {
  AssertComApartmentType(ComApartmentType::NONE);
  EXPECT_DCHECK_DEATH(AssertComApartmentType(ComApartmentType::STA));
  EXPECT_DCHECK_DEATH(AssertComApartmentType(ComApartmentType::MTA));
}

TEST(ComInitUtil, AssertNoneApartmentTypeUninitialized) {
  // When COM is uninitialized, the TLS data will remain, but the apartment
  // status will be updated. This covers that case.
  {
    ScopedCOMInitializer com_initializer;
    ASSERT_TRUE(com_initializer.Succeeded());
  }
  AssertComApartmentType(ComApartmentType::NONE);
  EXPECT_DCHECK_DEATH(AssertComApartmentType(ComApartmentType::STA));
  EXPECT_DCHECK_DEATH(AssertComApartmentType(ComApartmentType::MTA));
}

TEST(ComInitUtil, AssertSTAApartmentType) {
  ScopedCOMInitializer com_initializer;
  EXPECT_DCHECK_DEATH(AssertComApartmentType(ComApartmentType::NONE));
  AssertComApartmentType(ComApartmentType::STA);
  EXPECT_DCHECK_DEATH(AssertComApartmentType(ComApartmentType::MTA));
}

TEST(ComInitUtil, AssertMTAApartmentType) {
  ScopedCOMInitializer com_initializer(ScopedCOMInitializer::kMTA);
  EXPECT_DCHECK_DEATH(AssertComApartmentType(ComApartmentType::NONE));
  EXPECT_DCHECK_DEATH(AssertComApartmentType(ComApartmentType::STA));
  AssertComApartmentType(ComApartmentType::MTA);
}

}  // namespace win
}  // namespace base
