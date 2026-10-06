// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"

#include "base/test/scoped_feature_list.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// The bind-time gate, which decides whether a service worker is handed a
// ClipboardHost at all. It answers only whether this class of context is ever
// eligible, so it needs no extension and no profile. Whether a given extension
// may write is util::HasClipboardWritePermission, covered in
// chrome/browser/extensions/extension_util_unittest.cc.
class ClipboardForServiceWorkerTest : public testing::Test {
 public:
  ClipboardForServiceWorkerTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kClipboardOnExtensionServiceWorker);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ClipboardForServiceWorkerTest, AllowedForExtensionOrigin) {
  EXPECT_TRUE(ChromeContentBrowserClientExtensionsPart::
                  IsClipboardAllowedForServiceWorker(url::Origin::Create(
                      GURL("chrome-extension://abcdefg/"))));
}

TEST_F(ClipboardForServiceWorkerTest, RefusedForWebOrigin) {
  EXPECT_FALSE(ChromeContentBrowserClientExtensionsPart::
                   IsClipboardAllowedForServiceWorker(
                       url::Origin::Create(GURL("https://example.com/"))));
}

TEST_F(ClipboardForServiceWorkerTest, RefusedWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(
      blink::features::kClipboardOnExtensionServiceWorker);

  EXPECT_FALSE(ChromeContentBrowserClientExtensionsPart::
                   IsClipboardAllowedForServiceWorker(url::Origin::Create(
                       GURL("chrome-extension://abcdefg/"))));
}

}  // namespace extensions
