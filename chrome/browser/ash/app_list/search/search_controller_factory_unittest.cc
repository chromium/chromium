// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_controller_factory.h"

#include "components/omnibox/browser/autocomplete_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

namespace {
TEST(SearchControllerFactoryTest, LauncherSearchProviderTypes) {
  const int types = LauncherSearchProviderTypes();
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_DOCUMENT);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_OPEN_TAB);
}
}  // namespace
}  // namespace app_list
