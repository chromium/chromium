// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/resources_util.h"

#include <stddef.h>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/grit/components_scaled_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/resources/grit/ui_resources.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Check IDRs from ui/chromeos/resources/ui_chromeos_resources.grd.
    {"IDR_LOGIN_DEFAULT_USER", IDR_LOGIN_DEFAULT_USER},
#endif
    // Unknown names should be invalid and return -1.
    {"foobar", -1},
    {"backstar", -1},
  };

  for (size_t i = 0; i < std::size(kCases); ++i)
    EXPECT_EQ(kCases[i].id, ResourcesUtil::GetThemeResourceId(kCases[i].name));
}
