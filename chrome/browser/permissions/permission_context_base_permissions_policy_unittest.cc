// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/contexts/geolocation_permission_context.h"
#include "components/permissions/contexts/midi_permission_context.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/geolocation/geolocation_permission_context_delegate_android.h"
#else
#include "chrome/browser/geolocation/geolocation_permission_context_delegate.h"
#endif

// Integration tests for querying permissions that have a permissions policy
// set. These tests are not meant to cover every edge case as the
// PermissionsPolicy class itself is tested thoroughly in
// permissions_policy_unittest.cc and in
// render_frame_host_permissions_policy_unittest.cc. Instead they are meant to
// ensure that integration with content::PermissionContextBase works correctly.
class PermissionContextBasePermissionsPolicyTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void EnableBlockMidiByDefault() {
    feature_list_.InitAndEnableFeature(blink::features::kBlockMidiByDefault);
  }
  PermissionContextBasePermissionsPolicyTest()
      : last_request_result_(CONTENT_SETTING_DEFAULT) {}

  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }

 protected:
  static constexpr const char* kOrigin1 = "https://google.com";
  static constexpr const char* kOrigin2 = "https://maps.google.com";

  content::RenderFrameHost* GetMainRFH(const char* origin) {
    content::RenderFrameHost* result = web_contents()->GetPrimaryMainFrame();
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, GURL(origin));
    return result;
  }

  content::RenderFrameHost* AddChildRFH(
      content::RenderFrameHost* parent,
      const char* origin,
      blink::mojom::PermissionsPolicyFeature feature =
          blink::mojom::PermissionsPolicyFeature::kNotFound) {
    blink::ParsedPermissionsPolicy frame_policy = {};
    if (feature != blink::mojom::PermissionsPolicyFeature::kNotFound) {
      frame_policy.emplace_back(
          feature,
          std::vector({*blink::OriginWithPossibleWildcards::FromOrigin(
              url::Origin::Create(GURL(origin)))}),
          /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false);
    }
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChildWithPolicy(
            "", frame_policy);
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, GURL(origin));
    return result;
  }

  // The header policy should only be set once on page load, so we refresh the
  // page to simulate that.
  void RefreshPageAndSetHeaderPolicy(
      content::RenderFrameHost** rfh,
      blink::mojom::PermissionsPolicyFeature feature,
      const std::vector<std::string>& origins) {
    content::RenderFrameHost* current = *rfh;
    auto navigation = content::NavigationSimulator::CreateRendererInitiated(
        current->GetLastCommittedURL(), current);
    std::vector<blink::OriginWithPossibleWildcards> parsed_origins;
    for (const std::string& origin : origins) {
      parsed_origins.emplace_back(
          *blink::OriginWithPossibleWildcards::FromOrigin(
              url::Origin::Create(GURL(origin))));
    }
    navigation->SetPermissionsPolicyHeader(
        {{feature, parsed_origins, /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false}});
    navigation->Commit();
    *rfh = navigation->GetFinalRenderFrameHost();
  }

  ContentSetting GetPermissionForFrame(permissions::PermissionContextBase* pcb,
                                       content::RenderFrameHost* rfh) {
    return permissions::PermissionUtil::PermissionStatusToContentSetting(
        pcb->GetPermissionStatus(
               rfh, rfh->GetLastCommittedURL(),
               web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL())
            .status);
  }

  ContentSetting RequestPermissionForFrame(
      permissions::PermissionContextBase* pcb,
      content::RenderFrameHost* rfh) {
    permissions::PermissionRequestID id(
        rfh, permission_request_id_generator_.GenerateNextId());
    pcb->RequestPermission(
        permissions::PermissionRequestData(pcb, id,
                                           /*user_gesture=*/true,
                                           rfh->GetLastCommittedURL()),
        base::BindOnce(&PermissionContextBasePermissionsPolicyTest::
                           RequestPermissionForFrameFinished,
                       base::Unretained(this)));
    EXPECT_NE(CONTENT_SETTING_DEFAULT, last_request_result_);
    ContentSetting result = last_request_result_;
    last_request_result_ = CONTENT_SETTING_DEFAULT;
    return result;
  }

  std::unique_ptr<permissions::GeolocationPermissionContext>
  MakeGeolocationPermissionContext() {
    return std::make_unique<permissions::GeolocationPermissionContext>(
        profile(),
#if BUILDFLAG(IS_ANDROID)
        std::make_unique<GeolocationPermissionContextDelegateAndroid>(profile())
#else
        std::make_unique<GeolocationPermissionContextDelegate>(profile())
#endif
    );
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  void RequestPermissionForFrameFinished(ContentSetting setting) {
    last_request_result_ = setting;
  }

  void SimulateNavigation(content::RenderFrameHost** rfh, const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation_simulator->Commit();
    *rfh = navigation_simulator->GetFinalRenderFrameHost();
  }

  ContentSetting last_request_result_;
  permissions::PermissionRequestID::RequestLocalId::Generator
      permission_request_id_generator_;
};

TEST_F(PermissionContextBasePermissionsPolicyTest, DefaultPolicy) {
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);

  // Midi is ask by default in the top level frame but not in subframes.
  permissions::MidiPermissionContext midi(profile());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&midi, parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetPermissionForFrame(&midi, child));

  // Geolocation is ask by default in top level frames but not in subframes.
  auto geolocation = MakeGeolocationPermissionContext();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionForFrame(geolocation.get(), parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionForFrame(geolocation.get(), child));

  // Notifications is ask by default in top level frames but not in subframes.
  NotificationPermissionContext notifications(profile());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&notifications, parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionForFrame(&notifications, child));
}

TEST_F(PermissionContextBasePermissionsPolicyTest,
       DefaultPolicyBlockMidiByDefault) {
  EnableBlockMidiByDefault();

  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);

  // Midi is disallowed by default in the top level frame and blocked in
  // subframes.
  permissions::MidiPermissionContext midi(profile());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&midi, parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetPermissionForFrame(&midi, child));
}

TEST_F(PermissionContextBasePermissionsPolicyTest, DisabledTopLevelFrame) {
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);

  // Disable midi in the top level frame.
  RefreshPageAndSetHeaderPolicy(
      &parent, blink::mojom::PermissionsPolicyFeature::kMidiFeature,
      std::vector<std::string>());
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);
  permissions::MidiPermissionContext midi(profile());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetPermissionForFrame(&midi, parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetPermissionForFrame(&midi, child));

  // Disable geolocation in the top level frame.
  RefreshPageAndSetHeaderPolicy(
      &parent, blink::mojom::PermissionsPolicyFeature::kGeolocation,
      std::vector<std::string>());
  child = AddChildRFH(parent, kOrigin2);
  auto geolocation = MakeGeolocationPermissionContext();
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionForFrame(geolocation.get(), parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionForFrame(geolocation.get(), child));
}

TEST_F(PermissionContextBasePermissionsPolicyTest, EnabledForChildFrame) {
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);

  // Enable midi for the child frame.
  content::RenderFrameHost* child = AddChildRFH(
      parent, kOrigin2, blink::mojom::PermissionsPolicyFeature::kMidiFeature);
  permissions::MidiPermissionContext midi(profile());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&midi, parent));
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&midi, child));

  // Enable geolocation for the child frame.
  child = AddChildRFH(parent, kOrigin2,
                      blink::mojom::PermissionsPolicyFeature::kGeolocation);
  auto geolocation = MakeGeolocationPermissionContext();
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionForFrame(geolocation.get(), parent));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionForFrame(geolocation.get(), child));
}

TEST_F(PermissionContextBasePermissionsPolicyTest,
       EnabledForChildFrameBlockMidiByDefault) {
  EnableBlockMidiByDefault();

  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);

  // Enable midi for the child frame.
  content::RenderFrameHost* child = AddChildRFH(
      parent, kOrigin2, blink::mojom::PermissionsPolicyFeature::kMidiFeature);
  permissions::MidiPermissionContext midi(profile());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&midi, parent));
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&midi, child));
}

TEST_F(PermissionContextBasePermissionsPolicyTest, RequestPermission) {
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                 CONTENT_SETTING_ALLOW);

  // Request geolocation in the top level frame, request should work.
  auto geolocation = MakeGeolocationPermissionContext();
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            RequestPermissionForFrame(geolocation.get(), parent));

  // Disable geolocation in the top level frame.
  RefreshPageAndSetHeaderPolicy(
      &parent, blink::mojom::PermissionsPolicyFeature::kGeolocation,
      std::vector<std::string>());

  // Request should fail.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            RequestPermissionForFrame(geolocation.get(), parent));
}
