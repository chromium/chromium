// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager.h"

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "device/vr/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/mock_location_settings.h"
#include "chrome/browser/geolocation/geolocation_permission_context_android.h"
#endif  // defined(OS_ANDROID)

using blink::mojom::PermissionStatus;
using content::PermissionType;

namespace {

class PermissionManagerTestingProfile final : public TestingProfile {
 public:
  PermissionManagerTestingProfile() {}
  ~PermissionManagerTestingProfile() override {}

  PermissionManager* GetPermissionControllerDelegate() override {
    return PermissionManagerFactory::GetForProfile(this);
  }

  DISALLOW_COPY_AND_ASSIGN(PermissionManagerTestingProfile);
};

}  // namespace

class PermissionManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  void OnPermissionChange(PermissionStatus permission) {
    if (!quit_closure_.is_null())
      quit_closure_.Run();
    callback_called_ = true;
    callback_result_ = permission;
  }

 protected:
  PermissionManagerTest()
      : url_("https://example.com"),
        other_url_("https://foo.com"),
        callback_called_(false),
        callback_result_(PermissionStatus::ASK) {}

  PermissionManager* GetPermissionControllerDelegate() {
    return profile_->GetPermissionControllerDelegate();
  }

  HostContentSettingsMap* GetHostContentSettingsMap() {
    return HostContentSettingsMapFactory::GetForProfile(profile_.get());
  }

  void CheckPermissionStatus(PermissionType type,
                             PermissionStatus expected) {
    EXPECT_EQ(expected, GetPermissionControllerDelegate()->GetPermissionStatus(
                            type, url_.GetOrigin(), url_.GetOrigin()));
  }

  void CheckPermissionResult(ContentSettingsType type,
                             ContentSetting expected_status,
                             PermissionStatusSource expected_status_source) {
    PermissionResult result =
        GetPermissionControllerDelegate()->GetPermissionStatus(
            type, url_.GetOrigin(), url_.GetOrigin());
    EXPECT_EQ(expected_status, result.content_setting);
    EXPECT_EQ(expected_status_source, result.source);
  }

  void SetPermission(ContentSettingsType type, ContentSetting value) {
    HostContentSettingsMapFactory::GetForProfile(profile_.get())
        ->SetContentSettingDefaultScope(url_, url_, type, std::string(), value);
  }

  int RequestPermission(PermissionType type,
                        content::RenderFrameHost* rfh,
                        const GURL& origin) {
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    int result = GetPermissionControllerDelegate()->RequestPermission(
        type, rfh, origin, true,
        base::Bind(&PermissionManagerTest::OnPermissionChange,
                   base::Unretained(this)));
    loop.Run();
    return result;
  }

  const GURL& url() const {
    return url_;
  }

  const GURL& other_url() const {
    return other_url_;
  }

  GURL google_base_url() const {
    return GURL(UIThreadSearchTermsData(profile_.get()).GoogleBaseURLValue());
  }

  bool callback_called() const {
    return callback_called_;
  }

  PermissionStatus callback_result() const { return callback_result_; }

  void Reset() {
    callback_called_ = false;
    callback_result_ = PermissionStatus::ASK;
  }

  bool PendingRequestsEmpty() {
    return GetPermissionControllerDelegate()->pending_requests_.IsEmpty();
  }

  // The header policy should only be set once on page load, so we refresh the
  // page to simulate that.
  void RefreshPageAndSetHeaderPolicy(content::RenderFrameHost** rfh,
                                     blink::mojom::FeaturePolicyFeature feature,
                                     const std::vector<std::string>& origins) {
    content::RenderFrameHost* current = *rfh;
    SimulateNavigation(&current, current->GetLastCommittedURL());
    std::vector<url::Origin> parsed_origins;
    for (const std::string& origin : origins)
      parsed_origins.push_back(url::Origin::Create(GURL(origin)));
    content::RenderFrameHostTester::For(current)->SimulateFeaturePolicyHeader(
        feature, parsed_origins);
    *rfh = current;
  }

  content::RenderFrameHost* AddChildRFH(content::RenderFrameHost* parent,
                                        const char* origin) {
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChild("");
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, GURL(origin));
    return result;
  }

 private:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_.reset(new PermissionManagerTestingProfile());
#if defined(OS_ANDROID)
    GeolocationPermissionContextAndroid* geolocation_permission_context_ =
        static_cast<GeolocationPermissionContextAndroid*>(
            GetPermissionControllerDelegate()->GetPermissionContext(
                CONTENT_SETTINGS_TYPE_GEOLOCATION));
    geolocation_permission_context_->SetLocationSettingsForTesting(
        std::unique_ptr<LocationSettings>(new MockLocationSettings()));
    MockLocationSettings::SetLocationStatus(
        true /* has_android_location_permission */,
        true /* is_system_location_setting_enabled */);
#endif
    NavigateAndCommit(url());
  }

  void TearDown() override {
    profile_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SimulateNavigation(content::RenderFrameHost** rfh, const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation_simulator->Commit();
    *rfh = navigation_simulator->GetFinalRenderFrameHost();
  }

  const GURL url_;
  const GURL other_url_;
  bool callback_called_;
  PermissionStatus callback_result_;
  base::Closure quit_closure_;
  std::unique_ptr<PermissionManagerTestingProfile> profile_;
};

TEST_F(PermissionManagerTest, GetPermissionStatusDefault) {
  CheckPermissionStatus(PermissionType::MIDI_SYSEX, PermissionStatus::ASK);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS, PermissionStatus::ASK);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::ASK);
#if defined(OS_ANDROID)
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        PermissionStatus::GRANTED);
#endif
}

TEST_F(PermissionManagerTest, GetPermissionStatusAfterSet) {
  SetPermission(CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS,
                        PermissionStatus::GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::MIDI_SYSEX, PermissionStatus::GRANTED);

#if defined(OS_ANDROID)
  SetPermission(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        PermissionStatus::GRANTED);
#endif
}

TEST_F(PermissionManagerTest, CheckPermissionResultDefault) {
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                        CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
#if defined(OS_ANDROID)
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);
#endif
}

TEST_F(PermissionManagerTest, CheckPermissionResultAfterSet) {
  SetPermission(CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

  SetPermission(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

  SetPermission(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

#if defined(OS_ANDROID)
  SetPermission(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);
#endif
}

TEST_F(PermissionManagerTest, SubscriptionDestroyedCleanlyWithoutUnsubscribe) {
  // Test that the PermissionManager shuts down cleanly with subscriptions that
  // haven't been removed, crbug.com/720071.
  GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, main_rfh(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));
}

TEST_F(PermissionManagerTest, SameTypeChangeNotifies) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, DifferentTypeChangeDoesNotNotify) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), GURL(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, ChangeAfterUnsubscribeDoesNotNotify) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());
}

TEST_F(PermissionManagerTest, DifferentPrimaryUrlDoesNotNotify) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      other_url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, DifferentSecondaryUrlDoesNotNotify) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), other_url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, WildCardPatternNotifies) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, ClearSettingsNotifies) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_GEOLOCATION);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, NewValueCorrectlyPassed) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, ChangeWithoutPermissionChangeDoesNotNotify) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, ChangesBackAndForth) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ASK);

  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  Reset();

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ASK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, ChangesBackAndForthWorker) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ASK);

  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, nullptr, url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  Reset();

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ASK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, SubscribeMIDIPermission) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::MIDI, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::ASK);
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, PermissionIgnoredCleanup) {
  content::WebContents* contents = web_contents();
  PermissionRequestManager::CreateForWebContents(contents);
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(contents);
  auto prompt_factory = std::make_unique<MockPermissionPromptFactory>(manager);

  NavigateAndCommit(url());

  GetPermissionControllerDelegate()->RequestPermission(
      PermissionType::VIDEO_CAPTURE, main_rfh(), url(), /*user_gesture=*/true,
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  EXPECT_FALSE(PendingRequestsEmpty());

  NavigateAndCommit(GURL("https://foobar.com"));

  EXPECT_FALSE(callback_called());
  EXPECT_TRUE(PendingRequestsEmpty());
}

// Check PermissionResult shows requests denied due to insecure origins.
TEST_F(PermissionManagerTest, InsecureOrigin) {
  GURL insecure_frame("http://www.example.com/geolocation");
  NavigateAndCommit(insecure_frame);

  PermissionResult result =
      GetPermissionControllerDelegate()->GetPermissionStatusForFrame(
          CONTENT_SETTINGS_TYPE_GEOLOCATION, web_contents()->GetMainFrame(),
          insecure_frame);

  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::INSECURE_ORIGIN, result.source);

  GURL secure_frame("https://www.example.com/geolocation");
  NavigateAndCommit(secure_frame);

  result = GetPermissionControllerDelegate()->GetPermissionStatusForFrame(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, web_contents()->GetMainFrame(),
      secure_frame);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
}

TEST_F(PermissionManagerTest, GetCanonicalOriginSearch) {
  const GURL google_com("https://www.google.com");
  const GURL google_de("https://www.google.de");
  const GURL other_url("https://other.url");
  const GURL google_base = google_base_url().GetOrigin();
  const GURL local_ntp = GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin();
  const GURL remote_ntp = GURL(std::string("chrome-search://") +
                               chrome::kChromeSearchRemoteNtpHost);
  const GURL other_chrome_search = GURL("chrome-search://not-local-ntp");
  const GURL top_level_ntp(chrome::kChromeUINewTabURL);

  // "Normal" URLs are not affected by GetCanonicalOrigin.
  EXPECT_EQ(google_com, GetPermissionControllerDelegate()->GetCanonicalOrigin(
                            google_com, google_com));
  EXPECT_EQ(google_de, GetPermissionControllerDelegate()->GetCanonicalOrigin(
                           google_de, google_de));
  EXPECT_EQ(other_url, GetPermissionControllerDelegate()->GetCanonicalOrigin(
                           other_url, other_url));
  EXPECT_EQ(google_base, GetPermissionControllerDelegate()->GetCanonicalOrigin(
                             google_base, google_base));

  // The local NTP URL gets mapped to the Google base URL.
  EXPECT_EQ(google_base, GetPermissionControllerDelegate()->GetCanonicalOrigin(
                             local_ntp, top_level_ntp));
  // However, other chrome-search:// URLs, including the remote NTP URL, are
  // not affected.
  EXPECT_EQ(remote_ntp, GetPermissionControllerDelegate()->GetCanonicalOrigin(
                            remote_ntp, top_level_ntp));
  EXPECT_EQ(google_com, GetPermissionControllerDelegate()->GetCanonicalOrigin(
                            google_com, top_level_ntp));
  EXPECT_EQ(other_chrome_search,
            GetPermissionControllerDelegate()->GetCanonicalOrigin(
                other_chrome_search, top_level_ntp));
}

TEST_F(PermissionManagerTest, GetCanonicalOriginPermissionDelegation) {
  const GURL requesting_origin("https://www.requesting.com");
  const GURL embedding_origin("https://www.google.de");
  const GURL extensions_requesting_origin(
      "chrome-extension://abcdefghijklmnopqrstuvxyz");

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(features::kPermissionDelegation);
    // Without permission delegation enabled the requesting origin should always
    // be returned.
    EXPECT_EQ(requesting_origin,
              GetPermissionControllerDelegate()->GetCanonicalOrigin(
                  requesting_origin, embedding_origin));
    EXPECT_EQ(extensions_requesting_origin,
              GetPermissionControllerDelegate()->GetCanonicalOrigin(
                  extensions_requesting_origin, embedding_origin));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(features::kPermissionDelegation);
    // With permission delegation, the embedding origin should be returned
    // except in the case of extensions.
    EXPECT_EQ(embedding_origin,
              GetPermissionControllerDelegate()->GetCanonicalOrigin(
                  requesting_origin, embedding_origin));
    EXPECT_EQ(extensions_requesting_origin,
              GetPermissionControllerDelegate()->GetCanonicalOrigin(
                  extensions_requesting_origin, embedding_origin));
  }
}

TEST_F(PermissionManagerTest, GetPermissionStatusDelegation) {
  const char* kOrigin1 = "https://example.com";
  const char* kOrigin2 = "https://google.com";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPermissionDelegation);

  NavigateAndCommit(GURL(kOrigin1));
  content::RenderFrameHost* parent = main_rfh();

  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);

  // By default the parent should be able to request access, but not the child.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              parent, GURL(kOrigin1))
                .content_setting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  // Enabling geolocation by FP should allow the child to request access also.
  RefreshPageAndSetHeaderPolicy(
      &parent, blink::mojom::FeaturePolicyFeature::kGeolocation,
      {kOrigin1, kOrigin2});
  child = AddChildRFH(parent, kOrigin2);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  // When the child requests location a prompt should be displayed for the
  // parent.
  PermissionRequestManager::CreateForWebContents(web_contents());
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  auto prompt_factory = std::make_unique<MockPermissionPromptFactory>(manager);
  prompt_factory->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  prompt_factory->DocumentOnLoadCompletedInMainFrame();

  RequestPermission(PermissionType::GEOLOCATION, child, GURL(kOrigin2));

  EXPECT_TRUE(prompt_factory->RequestOriginSeen(GURL(kOrigin1)));

  // Now the child frame should have location, as well as the parent frame.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              parent, GURL(kOrigin1))
                .content_setting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  // Revoking access from the parent should cause the child not to have access
  // either.
  GetPermissionControllerDelegate()->ResetPermission(
      PermissionType::GEOLOCATION, GURL(kOrigin1), GURL(kOrigin1));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              parent, GURL(kOrigin1))
                .content_setting);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  // If the parent changes its policy, the child should be blocked.
  RefreshPageAndSetHeaderPolicy(
      &parent, blink::mojom::FeaturePolicyFeature::kGeolocation, {kOrigin1});
  child = AddChildRFH(parent, kOrigin2);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              parent, GURL(kOrigin1))
                .content_setting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  prompt_factory.reset();
}

TEST_F(PermissionManagerTest, SubscribeWithPermissionDelegation) {
  const char* kOrigin1 = "https://example.com";
  const char* kOrigin2 = "https://google.com";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPermissionDelegation);

  NavigateAndCommit(GURL(kOrigin1));
  content::RenderFrameHost* parent = main_rfh();
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);

  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, child, GURL(kOrigin2),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));
  EXPECT_FALSE(callback_called());

  // Location should be blocked for the child because it's not delegated.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  // Allow access for the top level origin.
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  // The child's permission should still be block and no callback should be run.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  EXPECT_FALSE(callback_called());

  // Enabling geolocation by FP should allow the child to request access also.
  RefreshPageAndSetHeaderPolicy(
      &parent, blink::mojom::FeaturePolicyFeature::kGeolocation,
      {kOrigin1, kOrigin2});
  child = AddChildRFH(parent, kOrigin2);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, child, GURL(kOrigin2),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));
  EXPECT_FALSE(callback_called());

  // Blocking access to the parent should trigger the callback to be run for the
  // child also.
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}
