// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/chrome_omnibox_navigation_observer_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;

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
    : public ChromeRenderViewHostTestHarness,
      public content::WebContentsDelegate {
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
      content::NavigationHandle& navigation_handle,
      const AutocompleteMatch& alternative_nav_match = AutocompleteMatch(),
      network::mojom::URLLoaderFactory* loader_factory = nullptr) {
    AutocompleteMatch match;
    match.keyword = kTestKeyword;
    match.destination_url = GURL(kTestUrl);
    return new ChromeOmniboxNavigationObserverAndroid(
        navigation_handle, profile(), u" text", match, alternative_nav_match,
        loader_factory);
  }

  ChromeOmniboxNavigationObserverBase::AlternativeFetchState GetFetchState(
      const ChromeOmniboxNavigationObserverAndroid& observer) const {
    return observer.fetch_state();
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

  // Runs a navigation with an alternative match for `http://intranet/` whose
  // HEAD request completes with `head_status`, then destroys the handle so the
  // observer runs its destructor.
  void RunAlternativeNavProbe(net::HttpStatusCode head_status) {
    network::TestURLLoaderFactory loader_factory;
    auto handle = CreateHandle(200);
    AutocompleteMatch alt_match;
    alt_match.destination_url = GURL("http://intranet/");
    CreateObserver(*handle, alt_match, &loader_factory)
        ->DidFinishNavigation(handle.get());
    EXPECT_TRUE(loader_factory.SimulateResponseForPendingRequest(
        "http://intranet/", "", head_status));
  }

  void DismissEnqueuedMessage() {
    if (enqueued_message_) {
      enqueued_message_.ExtractAsDangling()->HandleDismissCallback(
          base::android::AttachCurrentThread(),
          static_cast<int>(messages::DismissReason::UNKNOWN));
    }
  }

  void ExpectMessageEnqueued() {
    EXPECT_CALL(message_dispatcher_bridge_,
                EnqueueMessage(_, web_contents(),
                               messages::MessageScopeType::NAVIGATION,
                               messages::MessagePriority::kNormal))
        .WillOnce(DoAll(SaveArg<0>(&enqueued_message_), Return(true)));
  }

  messages::MockMessageDispatcherBridge& message_dispatcher_bridge() {
    return message_dispatcher_bridge_;
  }
  messages::MessageWrapper* enqueued_message() { return enqueued_message_; }
  ShortcutsBackend* backend() { return backend_.get(); }

  MockShortcutsObserver shortcuts_observer_;

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;

  testing::NiceMock<messages::MockMessageDispatcherBridge>
      message_dispatcher_bridge_;
  raw_ptr<messages::MessageWrapper> enqueued_message_ = nullptr;
  scoped_refptr<ShortcutsBackend> backend_;
};

void ChromeOmniboxNavigationObserverAndroidTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  web_contents()->SetDelegate(this);
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void ChromeOmniboxNavigationObserverAndroidTest::TearDown() {
  DismissEnqueuedMessage();
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();

  if (backend_) {
    backend_->RemoveObserver(&shortcuts_observer_);
  }
}

content::WebContents*
ChromeOmniboxNavigationObserverAndroidTest::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  source->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  return source;
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

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       AlternativeNavMatchIssuesHeadRequest) {
  network::TestURLLoaderFactory test_url_loader_factory;
  auto handle = CreateHandle(200);
  AutocompleteMatch alt_match;
  alt_match.destination_url = GURL("http://intranet/");

  auto* observer = CreateObserver(*handle, alt_match, &test_url_loader_factory);
  EXPECT_EQ(ChromeOmniboxNavigationObserverBase::AlternativeFetchState::
                kFetchNotComplete,
            GetFetchState(*observer));
  EXPECT_TRUE(test_url_loader_factory.SimulateResponseForPendingRequest(
      "http://intranet/", ""));
  EXPECT_EQ(ChromeOmniboxNavigationObserverBase::AlternativeFetchState::
                kFetchSucceeded,
            GetFetchState(*observer));
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       AlternateNavMessageShownOnFetchSuccess) {
  ExpectMessageEnqueued();

  RunAlternativeNavProbe(net::HTTP_OK);

  ASSERT_TRUE(enqueued_message());
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_INTRANET_REDIRECT_MESSAGE_TITLE,
                                       u"http://intranet/"),
            enqueued_message()->GetTitle());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_INTRANET_REDIRECT_MESSAGE_PRIMARY_BUTTON),
      enqueued_message()->GetPrimaryButtonText());
  EXPECT_EQ(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFO_OUTLINE_LOGO_24DP),
      enqueued_message()->GetIconResourceId());
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       AlternateNavMessageNotShownOnFetchFailure) {
  EXPECT_CALL(message_dispatcher_bridge(), EnqueueMessage).Times(0);

  RunAlternativeNavProbe(net::HTTP_NOT_FOUND);
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       AlternateNavMessagePrimaryAction) {
  CreateBackend();
  ExpectMessageEnqueued();

  RunAlternativeNavProbe(net::HTTP_OK);
  ASSERT_TRUE(enqueued_message());
  ASSERT_EQ(1u, backend()->shortcuts_map().size());
  EXPECT_EQ(
      GURL(kTestUrl),
      backend()->shortcuts_map().begin()->second.match_core.destination_url);

  enqueued_message()->HandleActionClick(base::android::AttachCurrentThread());

  ASSERT_EQ(1u, backend()->shortcuts_map().size());
  EXPECT_EQ(
      GURL("http://intranet/"),
      backend()->shortcuts_map().begin()->second.match_core.destination_url);
  ASSERT_TRUE(navigation_controller()->GetPendingEntry());
  EXPECT_EQ(GURL("http://intranet/"),
            navigation_controller()->GetPendingEntry()->GetURL());
}

TEST_F(ChromeOmniboxNavigationObserverAndroidTest,
       AlternateNavMessagePrimaryActionAfterWebContentsDestroyed) {
  CreateBackend();
  ExpectMessageEnqueued();

  RunAlternativeNavProbe(net::HTTP_OK);
  ASSERT_TRUE(enqueued_message());

  DeleteContents();
  enqueued_message()->HandleActionClick(base::android::AttachCurrentThread());

  ASSERT_EQ(1u, backend()->shortcuts_map().size());
  EXPECT_EQ(
      GURL(kTestUrl),
      backend()->shortcuts_map().begin()->second.match_core.destination_url);
}
