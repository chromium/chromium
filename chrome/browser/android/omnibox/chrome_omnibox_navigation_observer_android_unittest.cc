// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/chrome_omnibox_navigation_observer_android.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Assign;
using ::testing::Mock;

namespace {

const std::u16string kTestKeyword(u"keyword");
constexpr char kTestUrl[] = "https://www.google.com";

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
  ChromeOmniboxNavigationObserverAndroidTest() = default;
  ~ChromeOmniboxNavigationObserverAndroidTest() override = default;

  content::NavigationController* navigation_controller() {
    return &(web_contents()->GetController());
  }

  // Returns a handle for a committed primary main frame navigation that
  // returned `response_code`. The handle is bound to the fixture's WebContents
  // because the observer reads the visible URL off it.
  std::unique_ptr<content::MockNavigationHandle> CreateHandle(
      int response_code) {
    auto handle =
        std::make_unique<content::MockNavigationHandle>(web_contents());
    handle->set_url(GURL(kTestUrl));
    handle->set_is_in_primary_main_frame(true);
    handle->set_has_committed(true);
    handle->set_response_headers(GetHeadersForResponseCode(response_code));
    return handle;
  }

  ChromeOmniboxNavigationObserverAndroid* CreateObserver(
      content::NavigationHandle& navigation_handle) {
    AutocompleteMatch match;
    match.keyword = kTestKeyword;
    return new ChromeOmniboxNavigationObserverAndroid(
        navigation_handle, profile(), u" text", match);
  }

  // Drives a committed navigation returning `response_code` through the
  // observer, and returns whether it was recorded in the Shortcuts backend.
  bool NavigationRecordsShortcut(int response_code) {
    bool recorded = false;
    EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged())
        .WillRepeatedly(Assign(&recorded, true));

    auto handle = CreateHandle(response_code);
    CreateObserver(*handle)->DidFinishNavigation(handle.get());

    Mock::VerifyAndClearExpectations(&shortcuts_observer_);
    return recorded;
  }

  // Registers a search engine for `kTestKeyword` and returns the service.
  TemplateURLService* AddSearchEngine(bool safe_for_autoreplace) {
    TemplateURLServiceFactoryTestUtil factory_util(profile());
    factory_util.VerifyLoad();
    TemplateURLData data;
    data.SetKeyword(kTestKeyword);
    data.safe_for_autoreplace = safe_for_autoreplace;
    factory_util.model()->Add(std::make_unique<TemplateURL>(data));
    EXPECT_TRUE(factory_util.model()->GetTemplateURLForKeyword(kTestKeyword));
    return factory_util.model();
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

TEST_F(ChromeOmniboxNavigationObserverAndroidTest, AllHttp200AreFine) {
  CreateBackend();

  for (int http_code = 200; http_code < 300; http_code++) {
    EXPECT_TRUE(NavigationRecordsShortcut(http_code)) << http_code;
  }
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       SelectNonHttp200CodesAreFine) {
  CreateBackend();

  for (int http_code = 300; http_code < 600; http_code++) {
    // HTTP 401 and 407 also indicate that the target address exists.
    bool is_eligible = (http_code == 401) || (http_code == 407);
    EXPECT_EQ(is_eligible, NavigationRecordsShortcut(http_code)) << http_code;
  }
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       CreateWithNullNavigationHandle) {
  CreateBackend();
  AutocompleteMatch match;
  match.keyword = kTestKeyword;

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kTestUrl), web_contents());
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

  auto handle = CreateHandle(200);
  handle->set_has_committed(false);

  EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged()).Times(0);
  CreateObserver(*handle)->DidFinishNavigation(handle.get());
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       NoResponseHeadersDidFinishNavigation) {
  CreateBackend();

  auto handle = CreateHandle(200);
  handle->set_response_headers(nullptr);

  EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged()).Times(0);
  CreateObserver(*handle)->DidFinishNavigation(handle.get());
}

// The observer only reacts to the navigation it was created for, not to other
// navigations in the same WebContents.
TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       UnrelatedNavigationDidFinishNavigation) {
  CreateBackend();

  auto observed_navigation = CreateHandle(200);
  auto unrelated_navigation = CreateHandle(200);
  ASSERT_NE(observed_navigation->GetNavigationId(),
            unrelated_navigation->GetNavigationId());

  EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged()).Times(0);
  CreateObserver(*observed_navigation)
      ->DidFinishNavigation(unrelated_navigation.get());
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       Http404DeletesAutoGeneratedSearchEngine) {
  TemplateURLService* service = AddSearchEngine(/*safe_for_autoreplace=*/true);

  auto handle = CreateHandle(404);
  CreateObserver(*handle)->DidFinishNavigation(handle.get());

  EXPECT_FALSE(service->GetTemplateURLForKeyword(kTestKeyword));
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       Http404KeepsUserCreatedSearchEngine) {
  TemplateURLService* service = AddSearchEngine(/*safe_for_autoreplace=*/false);

  auto handle = CreateHandle(404);
  CreateObserver(*handle)->DidFinishNavigation(handle.get());

  EXPECT_TRUE(service->GetTemplateURLForKeyword(kTestKeyword));
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest, ShortcutReport) {
  CreateBackend();
  AutocompleteMatch match;
  match.keyword = kTestKeyword;
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(u"text");

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kTestUrl), web_contents());
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
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(u"text");

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kTestUrl), web_contents());
  navigation->SetResponseHeaders(GetHeadersForResponseCode(200));

  navigation->Start();
  ChromeOmniboxNavigationObserverAndroid::Create(
      navigation->GetNavigationHandle(), profile(), u" text", match);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(shortcuts_observer_, OnShortcutsChanged()).Times(0);
  navigation->Commit();
}
