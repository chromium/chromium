// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_external_protocol_dialog.h"

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/arc/common/intent_helper/arc_intent_helper_mojo_delegate.h"
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#include "components/arc/common/test/fake_arc_icon_cache.h"
#include "components/arc/common/test/fake_arc_intent_helper_mojo.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/mock_sharing_service.h"
#include "components/sharing_message/proto/click_to_call_message.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using ::testing::Property;

namespace arc {

namespace {

SharingTargetDeviceInfo CreateFakeSharingTargetDeviceInfo(
    const std::string& guid) {
  return SharingTargetDeviceInfo(guid, "Test name",
                                 SharingDevicePlatform::kUnknown,
                                 /*pulse_interval=*/base::TimeDelta(),
                                 syncer::DeviceInfo::FormFactor::kUnknown,
                                 /*last_updated_timestamp=*/base::Time());
}

// Helper class to run tests that need a dummy WebContents and arc delegate.
class ArcExternalProtocolDialogTestUtils : public BrowserWithTestWindowTest {
 public:
  ArcExternalProtocolDialogTestUtils() = default;
  ArcExternalProtocolDialogTestUtils(
      const ArcExternalProtocolDialogTestUtils&) = delete;
  ArcExternalProtocolDialogTestUtils& operator=(
      const ArcExternalProtocolDialogTestUtils&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    arc_icon_cache_ = std::make_unique<FakeArcIconCache>();
    delegate_provider_ =
        std::make_unique<ArcIconCacheDelegateProvider>(arc_icon_cache_.get());
  }

  void TearDown() override { BrowserWithTestWindowTest::TearDown(); }

 protected:
  void CreateTab(bool started_from_arc) {
    AddTab(browser(), GURL("http://www.tests.com"));

    web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);
    if (started_from_arc) {
      web_contents_->SetUserData(
          &ArcWebContentsData::kArcTransitionFlag,
          std::make_unique<ArcWebContentsData>(web_contents_));
    }
  }

  bool WasTabStartedFromArc() {
    return GetAndResetSafeToRedirectToArcWithoutUserConfirmationFlagForTesting(
        web_contents_);
  }

  MockSharingService* CreateSharingService() {
    return static_cast<MockSharingService*>(
        SharingServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating([](content::BrowserContext* context) {
              return static_cast<std::unique_ptr<KeyedService>>(
                  std::make_unique<MockSharingService>());
            })));
  }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  base::test::ScopedFeatureList features_{kClickToCall};
  // Keep only one |WebContents| at a time.
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  std::unique_ptr<ArcIconCacheDelegate> arc_icon_cache_;
  std::unique_ptr<ArcIconCacheDelegateProvider> delegate_provider_;
};

// Creates a dummy GurlAndActivityInfo object.
GurlAndActivityInfo CreateEmptyGurlAndActivityInfo() {
  return std::make_pair(GURL(), ArcIntentHelperMojoDelegate::ActivityName(
                                    /*package_name=*/std::string(),
                                    /*activity_name=*/std::string()));
}

// Creates and returns a new IntentHandlerInfo object.
ArcIntentHelperMojoDelegate::IntentHandlerInfo Create(
    const std::string& name,
    const std::string& package_name,
    const std::string& activity_name,
    bool is_preferred,
    const GURL& fallback_url) {
  std::optional<std::string> url;
  if (!fallback_url.is_empty())
    url = fallback_url.spec();

  return ArcIntentHelperMojoDelegate::IntentHandlerInfo(
      std::move(name), std::move(package_name), std::move(activity_name),
      is_preferred, std::move(url));
}

}  // namespace

// Tests that when one app is passed to GetAction but the user hasn't selected
// it and |in_out_safe_to_bypass_ui| is true, the function returns
// HANDLE_URL_IN_ARC.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithOneAppBypassesIntentPicker) {
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("package", "com.google.package.name",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, /*fallback_url=*/GURL()));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(GURL("external-protocol:foo"), handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Tests that when one app is passed to GetAction but the user hasn't selected
// it and |in_out_safe_to_bypass_ui| is false, the function returns
// ASK_USER.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithOneAppDoesntBypassIntentPicker) {
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("package", "com.google.package.name",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, /*fallback_url=*/GURL()));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::ASK_USER,
      GetActionForTesting(GURL("external-protocol:foo"), handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_FALSE(in_out_safe_to_bypass_ui);
}

// Tests that when 2+ apps are passed to GetAction but the user hasn't selected
// any the function returns ASK_USER, independently of whether or not is marked
// as safe to bypass the ui.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoAppWontBypassIntentPicker) {
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("package", "com.google.package.name",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, /*fallback_url=*/GURL()));
  handlers.push_back(Create("package2", "com.google.package.name2",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, /*fallback_url=*/GURL()));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::ASK_USER,
      GetActionForTesting(GURL("external-protocol:foo"), handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_FALSE(in_out_safe_to_bypass_ui);

  in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::ASK_USER,
      GetActionForTesting(GURL("external-protocol:foo"), handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_FALSE(in_out_safe_to_bypass_ui);
}

// Tests that when one preferred app is passed to GetAction, the function
// returns HANDLE_URL_IN_ARC even if the user hasn't selected the app, safe to
// bypass the UI is not relevant for this context.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOnePreferredApp) {
  const GURL external_url("external-protocol:foo");
  const std::string package_name("com.google.package.name");
  const std::string activity_name("com.google.activity");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("package", package_name, activity_name,
                            /*is_preferred=*/true,
                            /*fallback_url=*/GURL()));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();

  bool in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(external_url, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(external_url, url_and_activity_name.first);
  EXPECT_EQ(package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);

  in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(external_url, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  // The flag was flipped since we have a preferred app.
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
  EXPECT_EQ(external_url, url_and_activity_name.first);
  EXPECT_EQ(package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
}

// Tests that when one app is passed to GetAction, the user has already selected
// it, the function returns HANDLE_URL_IN_ARC. Since the user already selected
// safe to bypass ui it's always false.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOneAppSelected) {
  const GURL external_url("external-protocol:foo");
  const std::string package_name("com.google.package.name");
  const std::string activity_name("fake_activity_name");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("package", package_name, activity_name,
                            /*is_preferred=*/false,
                            /*fallback_url=*/GURL()));

  constexpr size_t kSelection = 0;
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(external_url, handlers, kSelection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(external_url, url_and_activity_name.first);
  EXPECT_EQ(package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
  EXPECT_FALSE(in_out_safe_to_bypass_ui);

  in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(external_url, handlers, kSelection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(external_url, url_and_activity_name.first);
  EXPECT_EQ(package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
  EXPECT_FALSE(in_out_safe_to_bypass_ui);
}

// Tests the same as TestGetActionWithOnePreferredApp but with two apps.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithOnePreferredAppAndOneOther) {
  const GURL external_url("external-protocol:foo");
  const std::string package_name("com.google.package2.name");
  const std::string activity_name("fake_activity_name2");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("package", "com.google.package.name",
                            "fake_activity_name",
                            /*is_preferred=*/false, /*fallback_url=*/GURL()));
  handlers.push_back(Create("package2", package_name, activity_name,
                            /*is_preferred=*/true,
                            /*fallback_url=*/GURL()));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  // For cases with 2+ apps it doesn't matter whether it was marked as safe to
  // bypass or not, it will only check for user's preferrences.
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(external_url, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(external_url, url_and_activity_name.first);
  EXPECT_EQ(package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
  // It is expected to correct the flag to true, regardless of the initial
  // value, since there is a preferred app.
  EXPECT_TRUE(in_out_safe_to_bypass_ui);

  in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(external_url, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(external_url, url_and_activity_name.first);
  EXPECT_EQ(package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Tests that HANDLE_URL_IN_ARC is returned for geo: URL. The URL is special in
// that intent_helper (i.e. the Chrome proxy) can handle it but Chrome cannot.
// We have to send such a URL to intent_helper to let the helper rewrite the
// URL to https://maps.google.com/?latlon=xxx which Chrome can handle. Since the
// url needs to be fixed in ARC first, safe to bypass doesn't modify this
// behavior.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithGeoUrl) {
  const GURL geo_url("geo:37.7749,-122.4194");

  const std::string activity_name("chrome_activity_name");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            activity_name,
                            /*is_preferred=*/true,
                            /*fallback_url=*/GURL()));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(geo_url, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(geo_url, url_and_activity_name.first);
  EXPECT_EQ(kArcIntentHelperPackageName,
            url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
  // Value will be corrected as in previous scenarios.
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Tests that OPEN_URL_IN_CHROME is returned when a handler with a fallback http
// URL and kArcIntentHelperPackageName is passed to GetAction, even if the
// handler is not a preferred one.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOneFallbackUrl) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");
  const std::string activity_name("fake_activity_name");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            activity_name,
                            /*is_preferred=*/false, fallback_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();

  // Since the navigation is intended to stay in Chrome the UI is bypassed.
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::OPEN_URL_IN_CHROME,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(kArcIntentHelperPackageName,
            url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);

  in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::OPEN_URL_IN_CHROME,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(kArcIntentHelperPackageName,
            url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Tests the same with https and is_preferred == true.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOnePreferredFallbackUrl) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=https://zxing.org;end");
  const GURL fallback_url("https://zxing.org");
  const std::string activity_name("fake_activity_name");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            activity_name,
                            /*is_preferred=*/true, fallback_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();

  // Safe to bypass should be marked as true in the end, since the
  // OPEN_URL_IN_CHROME actually bypasses the UI, regardless of the flag.
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::OPEN_URL_IN_CHROME,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(kArcIntentHelperPackageName,
            url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);

  // Changing the flag will not modify the outcome.
  in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::OPEN_URL_IN_CHROME,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(kArcIntentHelperPackageName,
            url_and_activity_name.second.package_name);
  EXPECT_EQ(activity_name, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Tests that ASK_USER is returned when two handlers with fallback URLs are
// passed to GetAction. This may happen when the user has installed a 3rd party
// browser app, and then clicks a intent: URI with a http fallback.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithTwoFallbackUrls) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Other browser", "com.other.browser",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, fallback_url));
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, fallback_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::ASK_USER,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_FALSE(in_out_safe_to_bypass_ui);
}

// Tests the same but set Chrome as a preferred app. In this case, ASK_USER
// shouldn't be returned.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoFallbackUrlsChromePreferred) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");
  const std::string chrome_activity("chrome_activity");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Other browser", "com.other.browser",
                            "fake_activity",
                            /*is_preferred=*/false, fallback_url));
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            chrome_activity,
                            /*is_preferred=*/true, fallback_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::OPEN_URL_IN_CHROME,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(kArcIntentHelperPackageName,
            url_and_activity_name.second.package_name);
  EXPECT_EQ(chrome_activity, url_and_activity_name.second.activity_name);
  // Remember that this flag gets fixed under the presence of a preferred app.
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Tests the same but set "other browser" as a preferred app. In this case,
// ASK_USER shouldn't be returned either.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoFallbackUrlsOtherBrowserPreferred) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");
  const std::string package_name = "com.other.browser";
  const std::string chrome_activity_name("chrome_activity_name");
  const std::string other_activity_name("other_activity_name");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Other browser", package_name, other_activity_name,
                            /*is_preferred=*/true, fallback_url));
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            chrome_activity_name,
                            /*is_preferred=*/false, fallback_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(other_activity_name, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Tests the same but set Chrome as a user-selected app.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoFallbackUrlsChromeSelected) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");
  const std::string chrome_activity_name("chrome_activity");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Other browser", "com.other.browser",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, fallback_url));
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            chrome_activity_name,
                            /*is_preferred=*/false, fallback_url));

  constexpr size_t kSelection = 1;  // Chrome
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::OPEN_URL_IN_CHROME,
      GetActionForTesting(intent_url_with_fallback, handlers, kSelection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(kArcIntentHelperPackageName,
            url_and_activity_name.second.package_name);
  EXPECT_EQ(chrome_activity_name, url_and_activity_name.second.activity_name);
  EXPECT_FALSE(in_out_safe_to_bypass_ui);
}

// Tests the same but set "other browser" as a preferred app.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoFallbackUrlsOtherBrowserSelected) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");
  const std::string package_name = "com.other.browser";
  const std::string other_activity_name("other_activity_name");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Other browser", package_name, other_activity_name,
                            /*is_preferred=*/false, fallback_url));
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            "chrome_activity",
                            /*is_preferred=*/false, fallback_url));

  constexpr size_t kSelection = 0;  // the other browser
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  // Already selected app index, output should be corrected to false.
  bool in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(intent_url_with_fallback, handlers, kSelection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(other_activity_name, url_and_activity_name.second.activity_name);
  EXPECT_FALSE(in_out_safe_to_bypass_ui);
}

// Tests that HANDLE_URL_IN_ARC is returned when a handler with a fallback
// market: URL is passed to GetAction iff the flag to bypass the UI is set,
// otherwise UI will prompt to ASK_USER.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOneMarketFallbackUrl) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Play Store", "com.google.play.store",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, fallback_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_TRUE(in_out_safe_to_bypass_ui);

  in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::ASK_USER,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_FALSE(in_out_safe_to_bypass_ui);
}

// Tests the same but with is_preferred == true.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithOnePreferredMarketFallbackUrl) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");
  const std::string play_store_package_name = "com.google.play.store";
  const std::string play_store_activity("play_store_activity");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Play Store", play_store_package_name,
                            play_store_activity,
                            /*is_preferred=*/true, fallback_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(play_store_package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(play_store_activity, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);

  in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(play_store_package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(play_store_activity, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Tests the same but with an app_seleteced_index.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithOneSelectedMarketFallbackUrl) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");
  const std::string play_store_package_name = "com.google.play.store";
  const std::string play_store_activity("play_store_activity");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Play Store", play_store_package_name,
                            play_store_activity,
                            /*is_preferred=*/false, fallback_url));

  constexpr size_t kSelection = 0;
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  // App already selected, it doesn't really makes sense to call GetAction with
  // |in_out_safe_to_bypass_ui| set to true here.
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(intent_url_with_fallback, handlers, kSelection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(play_store_package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(play_store_activity, url_and_activity_name.second.activity_name);
  EXPECT_FALSE(in_out_safe_to_bypass_ui);
}

// Tests that HANDLE_URL_IN_ARC is returned when a handler with a fallback
// market: URL is passed to GetAction.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithOneMarketFallbackUrlBypassIntentPicker) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Play Store", "com.google.play.store",
                            "play_store_activity", /*is_preferred=*/false,
                            fallback_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = true;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Tests that ASK_USER is returned when two handlers with fallback market: URLs
// are passed to GetAction. Unlike the two browsers case, this rarely happens on
// the user's device, though.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithTwoMarketFallbackUrls) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Play Store", "com.google.play.store",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, fallback_url));
  handlers.push_back(Create("Other Store app", "com.other.play.store",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, fallback_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::ASK_USER,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_FALSE(in_out_safe_to_bypass_ui);
}

// Tests the same, but make the second handler a preferred one.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoMarketFallbackUrlsOnePreferred) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");
  const std::string play_store_package_name = "com.google.play.store";
  const std::string play_store_activity("play.store.act1");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Other Store app", "com.other.play.store",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, fallback_url));
  handlers.push_back(Create("Play Store", play_store_package_name,
                            play_store_activity,
                            /*is_preferred=*/true, fallback_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(play_store_package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(play_store_activity, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Tests the same, but make the second handler a selected one.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoMarketFallbackUrlsOneSelected) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");
  const std::string play_store_package_name = "com.google.play.store";
  const std::string play_store_activity("play.store.act1");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Other Store app", "com.other.play.store",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, fallback_url));
  handlers.push_back(Create("Play Store", play_store_package_name,
                            play_store_activity,
                            /*is_preferred=*/false, fallback_url));

  const size_t kSelection = 1;  // Play Store
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  // After selection doesn't really makes sense to check this value.
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(intent_url_with_fallback, handlers, kSelection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(fallback_url, url_and_activity_name.first);
  EXPECT_EQ(play_store_package_name, url_and_activity_name.second.package_name);
  EXPECT_EQ(play_store_activity, url_and_activity_name.second.activity_name);
}

// Tests the case where geo: URL is returned as a fallback. This should never
// happen because intent_helper ignores such a fallback, but just in case.
// GetAction shouldn't crash at least.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithGeoUrlAsFallback) {
  // Note: geo: as a browser fallback is banned in the production code.
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=geo:37.7749,-122.4194;end");
  const GURL geo_url("geo:37.7749,-122.4194");
  const std::string chrome_activity("chrome.activity");

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            chrome_activity,
                            /*is_preferred=*/true, geo_url));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = false;
  // GetAction shouldn't return OPEN_URL_IN_CHROME because Chrome doesn't
  // directly support geo:.
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(intent_url_with_fallback, handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_EQ(geo_url, url_and_activity_name.first);
  EXPECT_EQ(kArcIntentHelperPackageName,
            url_and_activity_name.second.package_name);
  EXPECT_EQ(chrome_activity, url_and_activity_name.second.activity_name);
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

// Test that GetUrlToNavigateOnDeactivate returns an empty GURL when |handlers|
// is empty.
TEST(ArcExternalProtocolDialogTest, TestGetUrlToNavigateOnDeactivateEmpty) {
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  EXPECT_EQ(GURL(), GetUrlToNavigateOnDeactivateForTesting(handlers));
}

// Test that GetUrlToNavigateOnDeactivate returns an empty GURL when |handlers|
// only contains a (non-Chrome) app.
TEST(ArcExternalProtocolDialogTest, TestGetUrlToNavigateOnDeactivateAppOnly) {
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  // On production, when |handlers| only contains app(s), the fallback field is
  // empty, but to make the test more reliable, use non-empty fallback URL.
  handlers.push_back(Create("App", "app.package",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, GURL("http://www")));
  EXPECT_EQ(GURL(), GetUrlToNavigateOnDeactivateForTesting(handlers));
}

// Test that GetUrlToNavigateOnDeactivate returns an empty GURL when |handlers|
// only contains (non-Chrome) apps.
TEST(ArcExternalProtocolDialogTest, TestGetUrlToNavigateOnDeactivateAppsOnly) {
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  // On production, when |handlers| only contains app(s), the fallback field is
  // empty, but to make the test more reliable, use non-empty fallback URL.
  handlers.push_back(Create("App1", "app1.package",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, GURL("http://www")));
  handlers.push_back(Create("App2", "app2.package",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, GURL("http://www")));
  EXPECT_EQ(GURL(), GetUrlToNavigateOnDeactivateForTesting(handlers));
}

// Test that GetUrlToNavigateOnDeactivate returns an empty GURL when |handlers|
// contains Chrome, but it's not for http(s).
TEST(ArcExternalProtocolDialogTest, TestGetUrlToNavigateOnDeactivateGeoUrl) {
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create(
      "Chrome", kArcIntentHelperPackageName, /*activity_name=*/std::string(),
      /*is_preferred=*/false, GURL("geo:37.4220,-122.0840")));
  EXPECT_EQ(GURL(), GetUrlToNavigateOnDeactivateForTesting(handlers));
}

// Test that GetUrlToNavigateOnDeactivate returns non-empty GURL when |handlers|
// contains Chrome and an app.
TEST(ArcExternalProtocolDialogTest,
     TestGetUrlToNavigateOnDeactivateChromeAndApp) {
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  // On production, all handlers have the same fallback URL, but to make sure
  // that "Chrome" is actually selected by the function, use different URLs.
  handlers.push_back(Create("A browser app", "browser.app.package",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, GURL("http://www1/")));
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, GURL("http://www2/")));
  handlers.push_back(Create("Yet another browser app",
                            "yet.another.browser.app.package",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, GURL("http://www3/")));

  EXPECT_EQ(GURL("http://www2/"),
            GetUrlToNavigateOnDeactivateForTesting(handlers));
}

// Does the same with https, just in case.
TEST(ArcExternalProtocolDialogTest,
     TestGetUrlToNavigateOnDeactivateChromeAndAppHttps) {
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("A browser app", "browser.app.package",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, GURL("https://www1/")));
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, GURL("https://www2/")));
  handlers.push_back(Create("Yet another browser app",
                            "yet.another.browser.app.package",
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, GURL("https://www3/")));

  EXPECT_EQ(GURL("https://www2/"),
            GetUrlToNavigateOnDeactivateForTesting(handlers));
}

// Checks that the flag is correctly attached to the current tab.
TEST_F(ArcExternalProtocolDialogTestUtils, TestTabIsStartedFromArc) {
  CreateTab(/*started_from_arc=*/true);

  EXPECT_TRUE(WasTabStartedFromArc());
}

// Tests the same as the previous, just for when the data is not attached to the
// tab.
TEST_F(ArcExternalProtocolDialogTestUtils, TestTabIsNotStartedFromArc) {
  CreateTab(/*started_from_arc=*/false);

  EXPECT_FALSE(WasTabStartedFromArc());
}

// Tests that IsChromeAnAppCandidate works as intended.
TEST(ArcExternalProtocolDialogTest, TestIsChromeAnAppCandidate) {
  // First 3 cases are valid, just switching the position of Chrome.
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(
      Create("fake app 1", "fake.app.package", /*activity_name=*/std::string(),
             /*is_preferred=*/false, GURL("https://www.fo.com")));
  handlers.push_back(
      Create("fake app 2", "fake.app.package2", /*activity_name=*/std::string(),
             /*is_preferred=*/false, GURL("https://www.bar.com")));
  handlers.push_back(Create("Chrome", kArcIntentHelperPackageName,
                            /*activity_name=*/std::string(),
                            /*is_preferred=*/false, GURL("https://www/")));
  EXPECT_TRUE(IsChromeAnAppCandidateForTesting(handlers));

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers2;
  handlers2.push_back(
      Create("fake app 1", "fake.app.package", /*activity_name=*/std::string(),
             /*is_preferred=*/false, GURL("https://www.fo.com")));
  handlers2.push_back(Create("Chrome", kArcIntentHelperPackageName,
                             /*activity_name=*/std::string(),
                             /*is_preferred=*/false, GURL("https://www/")));
  handlers2.push_back(
      Create("fake app 2", "fake.app.package2", /*activity_name=*/std::string(),
             /*is_preferred=*/false, GURL("https://www.bar.com")));
  EXPECT_TRUE(IsChromeAnAppCandidateForTesting(handlers2));

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers3;
  handlers3.push_back(Create("Chrome", kArcIntentHelperPackageName,
                             /*activity_name=*/std::string(),
                             /*is_preferred=*/false, GURL("https://www/")));
  handlers3.push_back(
      Create("fake app 1", "fake.app.package", /*activity_name=*/std::string(),
             /*is_preferred=*/false, GURL("https://www.fo.com")));
  handlers3.push_back(
      Create("fake app 2", "fake.app.package2", /*activity_name=*/std::string(),
             /*is_preferred=*/false, GURL("https://www.bar.com")));
  EXPECT_TRUE(IsChromeAnAppCandidateForTesting(handlers3));

  // Only non-Chrome apps.
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers4;
  handlers4.push_back(
      Create("fake app 1", "fake.app.package", /*activity_name=*/std::string(),
             /*is_preferred=*/false, GURL("https://www.fo.com")));
  handlers4.push_back(
      Create("fake app 2", "fake.app.package2", /*activity_name=*/std::string(),
             /*is_preferred=*/false, GURL("https://www.bar.com")));
  handlers4.push_back(Create("fake app 3", "fake.app.package3",
                             /*activity_name=*/std::string(),
                             /*is_preferred=*/false, GURL("https://www/")));
  EXPECT_FALSE(IsChromeAnAppCandidateForTesting(handlers4));

  // Empty vector case.
  EXPECT_FALSE(IsChromeAnAppCandidateForTesting(
      std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo>()));
}

// Tests that when one app is passed to GetAction and it's for ARC IME, the
// picker won't be triggered.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithArcImeSettingsActivityBypassesIntentPicker) {
  constexpr char kPackageForOpeningArcImeSettingsPage[] =
      "org.chromium.arc.applauncher";
  constexpr char kActivityForOpeningArcImeSettingsPage[] =
      "org.chromium.arc.applauncher.InputMethodSettingsActivity";

  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  handlers.push_back(Create("ARC IME settings",
                            kPackageForOpeningArcImeSettingsPage,
                            kActivityForOpeningArcImeSettingsPage,
                            /*is_preferred=*/false, /*fallback_url=*/GURL()));

  const size_t no_selection = handlers.size();
  GurlAndActivityInfo url_and_activity_name = CreateEmptyGurlAndActivityInfo();
  bool in_out_safe_to_bypass_ui = false;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(GURL("intent:foo"), handlers, no_selection,
                          &url_and_activity_name, &in_out_safe_to_bypass_ui));
  EXPECT_TRUE(in_out_safe_to_bypass_ui);
}

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// Tests that clicking on a device calls through to SharingService.
TEST_F(ArcExternalProtocolDialogTestUtils, TestSelectDeviceForTelLink) {
  CreateTab(/*started_from_arc=*/false);

  std::string device_guid = "device_guid";
  MockSharingService* sharing_service = CreateSharingService();
  std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers;
  std::vector<SharingTargetDeviceInfo> devices;
  devices.push_back(CreateFakeSharingTargetDeviceInfo(device_guid));

  GURL phone_number("tel:073%2099%209999%2099");

  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_click_to_call_message()->set_phone_number(
      phone_number.GetContent());
  EXPECT_CALL(*sharing_service,
              SendMessageToDevice(
                  Property(&SharingTargetDeviceInfo::guid, device_guid),
                  testing::_, ProtoEquals(sharing_message), testing::_));

  OnIntentPickerClosedForTesting(
      web_contents()->GetWeakPtr(), phone_number,
      /*safe_to_bypass_ui=*/true, std::move(handlers),
      std::make_unique<FakeArcIntentHelperMojo>(), std::move(devices),
      /*selected_app_package=*/device_guid, apps::PickerEntryType::kDevice,
      apps::IntentPickerCloseReason::OPEN_APP, /*should_persist=*/false);
}

TEST_F(ArcExternalProtocolDialogTestUtils, TestDialogWithoutAppsWithDevices) {
  CreateTab(/*started_from_arc=*/false);

  MockSharingService* sharing_service = CreateSharingService();
  std::vector<SharingTargetDeviceInfo> devices;
  devices.push_back(CreateFakeSharingTargetDeviceInfo("device_guid"));

  EXPECT_CALL(*sharing_service, GetDeviceCandidates(testing::_))
      .WillOnce(testing::Return(testing::ByMove(std::move(devices))));

  base::RunLoop run_loop;
  ClickToCallUiController::GetOrCreateFromWebContents(web_contents())
      ->set_on_dialog_shown_closure_for_testing(run_loop.QuitClosure());

  bool handled = false;
  RunArcExternalProtocolDialog(
      GURL("tel:12341234"), /*initiating_origin=*/std::nullopt,
      web_contents()->GetWeakPtr(), ui::PAGE_TRANSITION_LINK,
      /*has_user_gesture=*/true, /*is_in_fenced_frame_tree=*/false,
      std::make_unique<FakeArcIntentHelperMojo>(),
      base::BindOnce([](bool* handled, bool result) { *handled = result; },
                     &handled));

  // Wait until the bubble is visible.
  run_loop.Run();
  EXPECT_TRUE(handled);
}

TEST_F(ArcExternalProtocolDialogTestUtils,
       TestDialogWithoutAppsWithDevicesInFencedFrameWithGesture) {
  CreateTab(/*started_from_arc=*/false);

  MockSharingService* sharing_service = CreateSharingService();
  std::vector<SharingTargetDeviceInfo> devices;
  devices.push_back(CreateFakeSharingTargetDeviceInfo("device_guid"));

  EXPECT_CALL(*sharing_service, GetDeviceCandidates(testing::_))
      .WillOnce(testing::Return(testing::ByMove(std::move(devices))));

  base::RunLoop run_loop;
  ClickToCallUiController::GetOrCreateFromWebContents(web_contents())
      ->set_on_dialog_shown_closure_for_testing(run_loop.QuitClosure());

  bool handled = false;
  RunArcExternalProtocolDialog(
      GURL("tel:12341234"), /*initiating_origin=*/std::nullopt,
      web_contents()->GetWeakPtr(), ui::PAGE_TRANSITION_AUTO_SUBFRAME,
      /*has_user_gesture=*/true, /*is_in_fenced_frame_tree=*/true,
      std::make_unique<FakeArcIntentHelperMojo>(),
      base::BindOnce([](bool* handled, bool result) { *handled = result; },
                     &handled));

  // Wait until the bubble is visible.
  run_loop.Run();
  EXPECT_TRUE(handled);
}

TEST_F(ArcExternalProtocolDialogTestUtils,
       TestDialogWithoutAppsWithDevicesInFencedFrameWithoutGesture) {
  CreateTab(/*started_from_arc=*/false);

  MockSharingService* sharing_service = CreateSharingService();
  EXPECT_CALL(*sharing_service, GetDeviceCandidates).Times(0);

  base::RunLoop run_loop;
  ClickToCallUiController::GetOrCreateFromWebContents(web_contents())
      ->set_on_dialog_shown_closure_for_testing(run_loop.QuitClosure());

  std::optional<bool> handled;
  RunArcExternalProtocolDialog(
      GURL("tel:12341234"), /*initiating_origin=*/std::nullopt,
      web_contents()->GetWeakPtr(), ui::PAGE_TRANSITION_AUTO_SUBFRAME,
      /*has_user_gesture=*/false, /*is_in_fenced_frame_tree=*/true,
      std::make_unique<FakeArcIntentHelperMojo>(),
      base::BindOnce(
          [](std::optional<bool>* handled, bool result) { *handled = result; },
          &handled));
  EXPECT_TRUE(handled.has_value());
  EXPECT_FALSE(*handled);
}

}  // namespace arc
