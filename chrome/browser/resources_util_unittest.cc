// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources_util.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/grit/components_scaled_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/resources/grit/ui_resources.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#endif

TEST(ResourcesUtil, SpotCheckIds) {
  const struct {
    const char* name;
    int id;
  } kCases[] = {
    // IDRs from chrome/app/theme/theme_resources.grd should be valid.
    {"IDR_ERROR_NETWORK_GENERIC", IDR_ERROR_NETWORK_GENERIC},
    // IDRs from ui/resources/ui_resources.grd should be valid.
    {"IDR_DEFAULT_FAVICON", IDR_DEFAULT_FAVICON},
#if defined(OS_CHROMEOS)
    // Check IDRs from ui/chromeos/resources/ui_chromeos_resources.grd.
    {"IDR_LOGIN_DEFAULT_USER", IDR_LOGIN_DEFAULT_USER},
#endif
    // Unknown names should be invalid and return -1.
    {"foobar", -1},
    {"backstar", -1},
  };

  for (size_t i = 0; i < base::size(kCases); ++i)
    EXPECT_EQ(kCases[i].id, ResourcesUtil::GetThemeResourceId(kCases[i].name));
}
