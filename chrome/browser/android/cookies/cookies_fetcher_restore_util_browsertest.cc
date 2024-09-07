// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/cookies/cookies_fetcher_restore_util.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/web_contents_state.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace cookie_fetcher_restore_util {

class CookiesFetcherRestoreUtilBrowserTest : public AndroidBrowserTest {
 protected:
  CookiesFetcherRestoreUtilBrowserTest() = default;
  ~CookiesFetcherRestoreUtilBrowserTest() override = default;

  void Navigate() {
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        embedded_test_server()->GetURL("/android/google.html")));
    Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext())
        ->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void AddCookie(std::string partition_key) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Profile* profile =
        Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext())
            ->GetPrimaryOTRProfile(/*create_if_needed=*/false);
    CookiesFetcherRestoreCookiesImpl(
        env, profile,
        jni_zero::JavaParamRef<jstring>(
            env, base::android::ConvertUTF8ToJavaString(env, "test").obj()),
        jni_zero::JavaParamRef<jstring>(
            env, base::android::ConvertUTF8ToJavaString(env, "test").obj()),
        jni_zero::JavaParamRef<jstring>(
            env,
            base::android::ConvertUTF8ToJavaString(env, "google.com").obj()),
        jni_zero::JavaParamRef<jstring>(
            env, base::android::ConvertUTF8ToJavaString(env, "/").obj()),
        /*creation=*/0, /*expiration=*/0, /*last_access=*/0,
        /*last_update=*/0, /*secure=*/true, /*httponly=*/false,
        /*same_site=*/0, /*priority=*/0,
        jni_zero::JavaParamRef<jstring>(
            env,
            base::android::ConvertUTF8ToJavaString(env, partition_key).obj()),
        /*source_scheme=*/2, /*source_port=*/-1, /*source_type=*/0);
  }

  bool HasCookie(std::string partition_key) {
    net::CookieList cookies_for_profile;
    {
      base::RunLoop loop;
      Profile* profile = Profile::FromBrowserContext(
                             GetActiveWebContents()->GetBrowserContext())
                             ->GetPrimaryOTRProfile(/*create_if_needed=*/false);
      GetCookieServiceClient(profile)->GetAllCookies(
          base::BindLambdaForTesting([&](const net::CookieList& cookies) {
            cookies_for_profile = cookies;
            loop.Quit();
          }));
      loop.Run();
    }
    if (cookies_for_profile.size() == 0u) {
      return false;
    }
    EXPECT_EQ(cookies_for_profile.size(), 1u);
    EXPECT_EQ(cookies_for_profile[0].Name(), "test");
    EXPECT_EQ(cookies_for_profile[0].PartitionKey(),
              *net::CookiePartitionKey::FromStorage(
                  partition_key, /*has_cross_site_ancestor=*/true));
    EXPECT_NE(cookies_for_profile[0].IsPartitioned(),
              partition_key.empty());
    return true;
  }

 private:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    PlatformBrowserTest::SetUpOnMainThread();
  }
};

// A cookie with an empty partition key should be restorable.
IN_PROC_BROWSER_TEST_F(CookiesFetcherRestoreUtilBrowserTest,
                       EmptyPartitionKey) {
  std::string partition_key = "";
  Navigate();
  AddCookie(partition_key);
  EXPECT_TRUE(HasCookie(partition_key));
}

// A cookie with an SchemefulSite partition key should be restorable.
IN_PROC_BROWSER_TEST_F(CookiesFetcherRestoreUtilBrowserTest,
                       ValidPartitionKey) {
  std::string partition_key = "https://google.com";
  Navigate();
  AddCookie(partition_key);
  EXPECT_TRUE(HasCookie(partition_key));
}

// A cookie with a malformed partition key should not be restorable.
IN_PROC_BROWSER_TEST_F(CookiesFetcherRestoreUtilBrowserTest,
                       InvalidPartitionKey) {
  std::string partition_key = "(╯°□°)╯︵ ┻━┻";
  Navigate();
  AddCookie(partition_key);
  EXPECT_FALSE(HasCookie(partition_key));
}

}  // namespace cookie_fetcher_restore_util
