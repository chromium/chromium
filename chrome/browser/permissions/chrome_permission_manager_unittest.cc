// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/permission_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/permissions_test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromePermissionManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(GetBrowserContext()));
  }

  void OnPermissionChange(PermissionStatus permission) {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
    callback_called_ = true;
    callback_result_ = permission;
  }

  bool callback_called() const { return callback_called_; }

  PermissionStatus callback_result() const { return callback_result_; }

  content::RenderFrameHost* AddChildRFH(
      content::RenderFrameHost* parent,
      const GURL& origin,
      blink::mojom::PermissionsPolicyFeature feature =
          blink::mojom::PermissionsPolicyFeature::kNotFound) {
    blink::ParsedPermissionsPolicy frame_policy = {};
    if (feature != blink::mojom::PermissionsPolicyFeature::kNotFound) {
      frame_policy.emplace_back(
          feature,
          std::vector{*blink::OriginWithPossibleWildcards::FromOrigin(
              url::Origin::Create(origin))},
          /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false);
    }
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChildWithPolicy(
            "", frame_policy);
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, origin);
    return result;
  }

  void SimulateNavigation(content::RenderFrameHost** rfh, const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation_simulator->Commit();
    *rfh = navigation_simulator->GetFinalRenderFrameHost();
  }

  void SetPermission(const GURL& url,
                     blink::PermissionType permission,
                     PermissionStatus status) {
    permissions::PermissionsClient::Get()
        ->GetSettingsMap(GetBrowserContext())
        ->SetContentSettingDefaultScope(
            url, url,
            permissions::PermissionUtil::PermissionTypeToContentSettingType(
                permission),
            permissions::PermissionUtil::PermissionStatusToContentSetting(
                status));
  }

 private:
  bool callback_called_ = false;
  PermissionStatus callback_result_ = PermissionStatus::ASK;
  base::OnceClosure quit_closure_;
};

TEST_F(ChromePermissionManagerTest, GetCanonicalOriginSearch) {
  const GURL google_com("https://www.google.com");
  const GURL google_de("https://www.google.de");
  const GURL other_url("https://other.url");
  const GURL google_base = GURL(UIThreadSearchTermsData().GoogleBaseURLValue())
                               .DeprecatedGetOriginAsURL();
  const GURL remote_ntp = GURL(std::string("chrome-search://") +
                               chrome::kChromeSearchRemoteNtpHost);
  const GURL other_chrome_search = GURL("chrome-search://not-local-ntp");
  const GURL top_level_ntp(chrome::kChromeUINewTabURL);
  const GURL webui_ntp = GURL(chrome::kChromeUINewTabPageURL);

  // "Normal" URLs are not affected by GetCanonicalOrigin.
  EXPECT_EQ(google_com,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_com, google_com));
  EXPECT_EQ(google_de,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_de, google_de));
  EXPECT_EQ(other_url,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, other_url, other_url));
  EXPECT_EQ(google_base,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_base, google_base));

  // The WebUI NTP URL gets mapped to the Google base URL.
  EXPECT_EQ(google_base,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, webui_ntp, top_level_ntp));

  // chrome-search://remote-ntp and other URLs are not affected.
  EXPECT_EQ(remote_ntp,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, remote_ntp, top_level_ntp));
  EXPECT_EQ(google_com,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_com, top_level_ntp));
  EXPECT_EQ(other_chrome_search,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, other_chrome_search,
                top_level_ntp));
}

TEST_F(ChromePermissionManagerTest, GetCanonicalOriginPermissionDelegation) {
  const GURL requesting_origin("https://www.requesting.com");
  const GURL embedding_origin("https://www.google.de");

  // The embedding origin should be returned except in the case of notifications
  // and, if they're enabled, extensions.
  EXPECT_EQ(embedding_origin, permissions::PermissionUtil::GetCanonicalOrigin(
                                  ContentSettingsType::GEOLOCATION,
                                  requesting_origin, embedding_origin));
  EXPECT_EQ(requesting_origin, permissions::PermissionUtil::GetCanonicalOrigin(
                                   ContentSettingsType::NOTIFICATIONS,
                                   requesting_origin, embedding_origin));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const GURL extensions_requesting_origin(
      "chrome-extension://abcdefghijklmnopqrstuvxyz");
  EXPECT_EQ(extensions_requesting_origin,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, extensions_requesting_origin,
                embedding_origin));
#endif
}

TEST_F(ChromePermissionManagerTest, SubscribeWithPermissionDelegation) {
  const char* kOrigin1 = "https://example.com";
  const char* kOrigin2 = "https://google.com";
  const GURL url1 = GURL(kOrigin1);
  const GURL url2 = GURL(kOrigin2);

  NavigateAndCommit(url1);
  content::RenderFrameHost* parent = main_rfh();
  content::RenderFrameHost* child = AddChildRFH(parent, url2);
  content::PermissionController* permission_controller =
      GetBrowserContext()->GetPermissionController();

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          permission_controller, blink::PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, child, url2,
          /*should_include_device_status=*/false,
          base::BindRepeating(&ChromePermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));
  EXPECT_FALSE(callback_called());

  // Location should be blocked for the child because it's not delegated.
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                blink::PermissionType::GEOLOCATION, child));

  // Allow access for the top level origin.
  SetPermission(url1, blink::PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);

  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                blink::PermissionType::GEOLOCATION, parent));

  // The child's permission should still be block and no callback should be run.
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                blink::PermissionType::GEOLOCATION, child));

  EXPECT_FALSE(callback_called());

  // Enabling geolocation by FP should allow the child to request access also.
  child = AddChildRFH(parent, url2,
                      blink::mojom::PermissionsPolicyFeature::kGeolocation);

  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                blink::PermissionType::GEOLOCATION, child));

  permission_controller->UnsubscribeFromPermissionStatusChange(subscription_id);
}
