// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/diacritics_checker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

namespace {

TEST(DiacriticsCheckerTest, CheckReportsThereAreSomeDiacritics) {
  EXPECT_TRUE(HasDiacritics(u"français"));
  EXPECT_TRUE(HasDiacritics(u"déjà"));
  EXPECT_TRUE(HasDiacritics(u"Español"));
  EXPECT_TRUE(HasDiacritics(u"École"));
  EXPECT_TRUE(HasDiacritics(u"cœur"));
  EXPECT_TRUE(HasDiacritics(u"København"));
  EXPECT_TRUE(HasDiacritics(u"ångström"));
  EXPECT_TRUE(HasDiacritics(u"Neuchâtel"));
  EXPECT_TRUE(HasDiacritics(u"jamón"));
  EXPECT_TRUE(HasDiacritics(u"NOËL"));
}

TEST(DiacriticsCheckerTest, CheckReportsThereAreNoDiacritics) {
  EXPECT_FALSE(HasDiacritics(u""));
  EXPECT_FALSE(HasDiacritics(u"   "));
  EXPECT_FALSE(HasDiacritics(u"francais"));
  EXPECT_FALSE(HasDiacritics(u"deja"));
  EXPECT_FALSE(HasDiacritics(u"Espanol"));
  EXPECT_FALSE(HasDiacritics(u"Ecole"));
  EXPECT_FALSE(HasDiacritics(u"coeur"));
  EXPECT_FALSE(HasDiacritics(u"Copenhagen"));
  EXPECT_FALSE(HasDiacritics(u"angstrom"));
  EXPECT_FALSE(HasDiacritics(u"Newcastle"));
  EXPECT_FALSE(HasDiacritics(u"jambon"));
  EXPECT_FALSE(HasDiacritics(u"Christmas"));
}

}  // namespace
}  // namespace app_list
