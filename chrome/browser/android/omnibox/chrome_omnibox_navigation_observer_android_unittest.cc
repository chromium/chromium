// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/chrome_omnibox_navigation_observer_android.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Return;

namespace {

const std::u16string kTestKeyword(u"Keyword");

scoped_refptr<net::HttpResponseHeaders> GetHeadersForResponseCode(int code) {
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      std::string("HTTP/1.1 ") + base::NumberToString(code) + " Message\r\n");
}

class MockShortcutsObserver
    : public ShortcutsBackend::ShortcutsBackendObserver {
 public:
  MOCK_METHOD0(OnShortcutsLoaded, void());
  MOCK_METHOD0(OnShortcutsChanged, void());
};

}  // namespace

class ChromeOmniboxNavigationObserverAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeOmniboxNavigationObserverAndroidTest(
      const ChromeOmniboxNavigationObserverAndroidTest&) = delete;
  ChromeOmniboxNavigationObserverAndroidTest& operator=(
      const ChromeOmniboxNavigationObserverAndroidTest&) = delete;

 protected:
  ChromeOmniboxNavigationObserverAndroidTest() {}
  ~ChromeOmniboxNavigationObserverAndroidTest() override {}

  content::NavigationController* navigation_controller() {
    return &(web_contents()->GetController());
  }

  ChromeOmniboxNavigationObserverAndroid* CreateObserver(
      content::NavigationHandle* navigation_handle) {
    std::u16string input_text = u" text";

    AutocompleteMatch match;
    match.keyword = kTestKeyword;
    return new ChromeOmniboxNavigationObserverAndroid(
        navigation_handle, profile(), input_text, match);
  }

  void CreateBackend() {
    ShortcutsBackendFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(
            &ShortcutsBackendFactory::BuildProfileNoDatabaseForTesting));

    backend_ = ShortcutsBackendFactory::GetForProfile(profile());
    ASSERT_TRUE(backend_.get());
    backend_->AddObserver(&shortcuts_observer_);
  }

  MockShortcutsObserver shortcuts_observer_;

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;

  scoped_refptr<ShortcutsBackend> backend_;
};

void ChromeOmniboxNavigationObserverAndroidTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
}

void ChromeOmniboxNavigationObserverAndroidTest::TearDown() {
  ChromeRenderViewHostTestHarness::TearDown();

  if (backend_) {
    backend_->RemoveObserver(&shortcuts_observer_);
  }
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest, NotCommitted) {
  content::MockNavigationHandle mock_handle(GURL("https://www.google.com"),
                                            nullptr);
  mock_handle.set_has_committed(false);
  mock_handle.set_response_headers(GetHeadersForResponseCode(200));

  ChromeOmniboxNavigationObserverAndroid* observer =
      CreateObserver(&mock_handle);
  EXPECT_FALSE(observer->NavigationEligible(&mock_handle));
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest, NoHeaders) {
  content::MockNavigationHandle mock_handle(GURL("https://www.google.com"),
                                            nullptr);
  mock_handle.set_has_committed(true);

  // Headers not instrumented.
  ChromeOmniboxNavigationObserverAndroid* observer =
      CreateObserver(&mock_handle);
  EXPECT_FALSE(observer->NavigationEligible(&mock_handle));
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest, AllHttp200AreFine) {
  for (int http_code = 200; http_code < 300; http_code++) {
    content::MockNavigationHandle mock_handle(GURL("https://www.google.com"),
                                              nullptr);
    mock_handle.set_has_committed(true);
    mock_handle.set_response_headers(GetHeadersForResponseCode(http_code));

    ChromeOmniboxNavigationObserverAndroid* observer =
        CreateObserver(&mock_handle);
    EXPECT_TRUE(observer->NavigationEligible(&mock_handle));
  }
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       SelectNonHttp200CodesAreFine) {
  for (int http_code = 300; http_code < 600; http_code++) {
    content::MockNavigationHandle mock_handle(GURL("https://www.google.com"),
                                              nullptr);
    mock_handle.set_has_committed(true);
    mock_handle.set_response_headers(GetHeadersForResponseCode(http_code));

    // Eligible codes:
    bool is_eligible = (http_code == 401) || (http_code == 407);
    ChromeOmniboxNavigationObserverAndroid* observer =
        CreateObserver(&mock_handle);
    EXPECT_EQ(is_eligible, observer->NavigationEligible(&mock_handle));
  }
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       CreateWithNullNavigationHandle) {
  CreateBackend();
  AutocompleteMatch match;
  match.keyword = kTestKeyword;

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.google.com"), web_contents());
  navigation->SetResponseHeaders(GetHeadersForResponseCode(200));

  navigation->Start();
  ChromeOmniboxNavigationObserverAndroid::Create(nullptr, profile(), u" text",
                                                 match);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged()).Times(0);
  navigation->Commit();
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       NotCommittedDidFinishNavigation) {
  CreateBackend();

  content::MockNavigationHandle mock_handle(GURL("https://www.google.com"),
                                            nullptr);
  mock_handle.set_is_in_primary_main_frame(true);
  mock_handle.set_has_committed(false);
  mock_handle.set_response_headers(GetHeadersForResponseCode(200));

  ChromeOmniboxNavigationObserverAndroid* observer =
      CreateObserver(&mock_handle);

  EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged()).Times(0);
  observer->DidFinishNavigation(&mock_handle);
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       NotInPrimaryMainFrameDidFinishNavigation) {
  CreateBackend();

  content::MockNavigationHandle mock_handle(GURL("https://www.google.com"),
                                            nullptr);
  mock_handle.set_is_in_primary_main_frame(false);
  mock_handle.set_has_committed(true);
  mock_handle.set_response_headers(GetHeadersForResponseCode(200));

  ChromeOmniboxNavigationObserverAndroid* observer =
      CreateObserver(&mock_handle);

  EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged()).Times(0);
  observer->DidFinishNavigation(&mock_handle);
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       NoResponseHeadersDidFinishNavigation) {
  CreateBackend();

  content::MockNavigationHandle mock_handle(GURL("https://www.google.com"),
                                            nullptr);
  mock_handle.set_is_in_primary_main_frame(true);
  mock_handle.set_has_committed(true);
  mock_handle.set_response_headers(nullptr);

  ChromeOmniboxNavigationObserverAndroid* observer =
      CreateObserver(&mock_handle);

  EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged()).Times(0);
  observer->DidFinishNavigation(&mock_handle);
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest, ShortcutReport) {
  CreateBackend();
  AutocompleteMatch match;
  match.keyword = kTestKeyword;

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.google.com"), web_contents());
  navigation->SetResponseHeaders(GetHeadersForResponseCode(200));

  navigation->Start();
  ChromeOmniboxNavigationObserverAndroid::Create(
      navigation->GetNavigationHandle(), profile(), u" text", match);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged());
  navigation->Commit();
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest, NoBackend) {
  AutocompleteMatch match;
  match.keyword = kTestKeyword;

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.google.com"), web_contents());
  navigation->SetResponseHeaders(GetHeadersForResponseCode(200));

  navigation->Start();
  ChromeOmniboxNavigationObserverAndroid::Create(
      navigation->GetNavigationHandle(), profile(), u" text", match);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged()).Times(0);
  navigation->Commit();
}
