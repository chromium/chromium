// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/test/test_support_jni_headers/OmniboxTestUtils_jni.h"
#else
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace extensions {

namespace {

void SendToOmniboxAndSubmit(BrowserWindowInterface* browser, const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  TabListInterface* tab_list = TabListInterface::From(browser);
  CHECK(tab_list);
  TabModel* tab_model = static_cast<TabModel*>(tab_list);
  chrome::android::Java_OmniboxTestUtils_loadUrlFromOmnibox(
      env, tab_model->GetJavaObject(), url.spec());
#else
  ui_test_utils::SendToOmniboxAndSubmit(browser, url.spec());
#endif
}

}  // namespace

using OmniboxNavigationBrowserTest = ExtensionBrowserTest;

// Tests that navigating to extension resource URLs (e.g. popup.html and
// manifest.json) from the omnibox loads the page. Regression test for
// crbug.com/496024074.
IN_PROC_BROWSER_TEST_F(OmniboxNavigationBrowserTest,
                       NavigateToExtensionResourceFromOmnibox) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Omnibox Navigation Test",
    "version": "1.0",
    "manifest_version": 3
  })");
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"),
                     "<!DOCTYPE html><html><body>Hello</body></html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Navigate to an extension HTML resource.
  {
    const GURL popup_url = extension->GetResourceURL("popup.html");
    content::TestNavigationObserver observer(web_contents);
    SendToOmniboxAndSubmit(browser_window_interface(), popup_url);
    observer.Wait();

    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(web_contents->GetLastCommittedURL(), popup_url);
  }

  // Navigate to the extension's manifest.json.
  {
    const GURL manifest_url = extension->GetResourceURL("manifest.json");
    content::TestNavigationObserver observer(web_contents);
    SendToOmniboxAndSubmit(browser_window_interface(), manifest_url);
    observer.Wait();

    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(web_contents->GetLastCommittedURL(), manifest_url);
  }
}

}  // namespace extensions
