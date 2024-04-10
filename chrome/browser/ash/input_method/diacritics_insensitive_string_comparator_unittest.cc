// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/diacritics_insensitive_string_comparator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {

TEST(DiacriticsInsensitiveStringComparatorTest, TestEqual) {
  DiacriticsInsensitiveStringComparator comparator;

  EXPECT_TRUE(comparator.Equal(u"français", u"francais"));
  EXPECT_TRUE(comparator.Equal(u"déjà", u"deja"));
  EXPECT_TRUE(comparator.Equal(u"Español", u"Espanol"));
  EXPECT_TRUE(comparator.Equal(u"École", u"Ecole"));
  EXPECT_TRUE(comparator.Equal(u"coeur", u"cœur"));
  EXPECT_TRUE(comparator.Equal(u"Kobenhavn", u"København"));
  EXPECT_TRUE(comparator.Equal(u"ångström", u"angstrom"));
  EXPECT_TRUE(comparator.Equal(u"Neuchatel", u"Neuchâtel"));
  EXPECT_TRUE(comparator.Equal(u"jamón", u"jamon"));
  EXPECT_TRUE(comparator.Equal(u"NOËL", u"NOEL"));
}

TEST(DiacriticsInsensitiveStringComparatorTest, TestNotEqual) {
  DiacriticsInsensitiveStringComparator comparator;

  EXPECT_FALSE(comparator.Equal(u"Français", u"français"));
  EXPECT_FALSE(comparator.Equal(u"Déjà", u"deja"));
  EXPECT_FALSE(comparator.Equal(u"español", u"français"));
  EXPECT_FALSE(comparator.Equal(u"École", u"ecole"));
  EXPECT_FALSE(comparator.Equal(u"coeur", u"œufs"));
  EXPECT_FALSE(comparator.Equal(u"København", u"Copenhagen"));
  EXPECT_FALSE(comparator.Equal(u"ångström", u"angstroms"));
  EXPECT_FALSE(comparator.Equal(u"Newcastle", u"Neuchâtel"));
  EXPECT_FALSE(comparator.Equal(u"jamón", u"jambon"));
  EXPECT_FALSE(comparator.Equal(u"Noël", u"Christmas"));
}

}  // namespace input_method
}  // namespace ash
