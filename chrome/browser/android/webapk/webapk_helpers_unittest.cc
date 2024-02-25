// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webapk {

// The first 2 examples are copied from
// chrome/browser/web_applications/web_app_helpers_unittest.cc's
// WebAppHelpers.GenerateAppId test to ensure desktop/mobile compatibility.
TEST(WebApkHelpers, GenerateAppIdFromManifestId) {
  EXPECT_EQ("fedbieoalmbobgfjapopkghdmhgncnaa",
            GenerateAppIdFromManifestId(
                GURL("https://www.chromestatus.com/features")));

  // The io2016 example is also walked through at
  // https://play.golang.org/p/VrIq_QKFjiV
  EXPECT_EQ("mjgafbdfajpigcjmkgmeokfbodbcfijl",
            GenerateAppIdFromManifestId(GURL("https://events.google.com/io2016/"
                                             "?utm_source=web_app_manifest")));

  EXPECT_EQ("ocjeedicdelkkoefdcgeopgiagdjbcng",
            GenerateAppIdFromManifestId(GURL("https://example.com/")));
}

}  // namespace webapk
