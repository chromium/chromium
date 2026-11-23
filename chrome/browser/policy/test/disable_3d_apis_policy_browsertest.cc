// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace policy {

namespace {

// Checks if WebGL is enabled in the given `WebContents`.
bool IsWebGLEnabled(content::WebContents* contents) {
  return content::EvalJs(contents,
                         "var canvas = document.createElement('canvas');"
                         "var context = canvas.getContext('webgl');"
                         "context != null;")
      .ExtractBool();
}

// Crashes and reloads the tab to get a new renderer.
void CrashAndReload(content::WebContents* contents) {
  content::CrashTab(contents);
  content::TestNavigationObserver observer(contents);
  contents->GetController().Reload(content::ReloadType::NORMAL, false);
  observer.Wait();
}

}  // namespace

// TODO(crbug.com/40684098): Deflake this test.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_Disable3DAPIs DISABLED_Disable3DAPIs
#else
#define MAYBE_Disable3DAPIs Disable3DAPIs
#endif
IN_PROC_BROWSER_TEST_F(PolicyTest, MAYBE_Disable3DAPIs) {
  ASSERT_TRUE(NavigateToUrl(GURL(url::kAboutBlankURL), this));
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  EXPECT_TRUE(contents->GetOrCreateWebPreferences().webgl1_enabled);
  EXPECT_TRUE(contents->GetOrCreateWebPreferences().webgl2_enabled);
  // Some test devices have no working WebGL, even without the policy.
  const bool webgl_available = IsWebGLEnabled(contents);
  // Disable with a policy.
  PolicyMap policies;
  SetPolicy(&policies, key::kDisable3DAPIs, base::Value(true));
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(contents->GetOrCreateWebPreferences().webgl1_enabled);
  EXPECT_FALSE(contents->GetOrCreateWebPreferences().webgl2_enabled);
  CrashAndReload(contents);
  EXPECT_FALSE(IsWebGLEnabled(contents));
  // Enable with a policy.
  SetPolicy(&policies, key::kDisable3DAPIs, base::Value(false));
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(contents->GetOrCreateWebPreferences().webgl1_enabled);
  EXPECT_TRUE(contents->GetOrCreateWebPreferences().webgl2_enabled);
  CrashAndReload(contents);
  EXPECT_EQ(IsWebGLEnabled(contents), webgl_available);
}

}  // namespace policy
