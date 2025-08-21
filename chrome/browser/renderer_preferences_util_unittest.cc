// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include <array>

#include "base/memory/raw_ptr.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/peerconnection/webrtc_ip_handling_policy.mojom-shared.h"

class RendererPreferencesUtilTest : public testing::Test {
 public:
  RendererPreferencesUtilTest() : pref_service_(profile_.GetPrefs()) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<PrefService> pref_service_;
};

TEST_F(RendererPreferencesUtilTest, WebRTCPostQuantumKeyAgreement) {
  std::array<std::optional<bool>, 3> webrtc_post_quantum_key_agreement_values =
      {std::nullopt, true, false};
  std::array<std::optional<bool>, 3> expected_values = {std::nullopt, true,
                                                        false};

  for (int i = 0; i < 3; i++) {
    if (webrtc_post_quantum_key_agreement_values[i]) {
      profile_.GetTestingPrefService()->SetManagedPref(
          prefs::kWebRTCPostQuantumKeyAgreement,
          std::make_unique<base::Value>(
              *webrtc_post_quantum_key_agreement_values[i]));
    }
    blink::RendererPreferences renderer_preferences;
    renderer_preferences_util::UpdateFromSystemSettings(&renderer_preferences,
                                                        &profile_);
    EXPECT_EQ(renderer_preferences.webrtc_post_quantum_key_agreement,
              expected_values[i]);
  }
}

TEST_F(RendererPreferencesUtilTest, WebRTCIPHandlingPolicy) {
  std::array<const char*, 5> webrtc_ip_handling_policy_values = {
      blink::kWebRTCIPHandlingDefault,
      blink::kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces,
      blink::kWebRTCIPHandlingDefaultPublicInterfaceOnly,
      blink::kWebRTCIPHandlingDisableNonProxiedUdp, "some_nonstandard_value"};

  std::array<blink::mojom::WebRtcIpHandlingPolicy, 5> expected_values = {
      blink::mojom::WebRtcIpHandlingPolicy::kDefault,
      blink::mojom::WebRtcIpHandlingPolicy::kDefaultPublicAndPrivateInterfaces,
      blink::mojom::WebRtcIpHandlingPolicy::kDefaultPublicInterfaceOnly,
      blink::mojom::WebRtcIpHandlingPolicy::kDisableNonProxiedUdp,
      blink::mojom::WebRtcIpHandlingPolicy::kDefault};

  for (int i = 0; i < 5; i++) {
    pref_service_->SetString(prefs::kWebRTCIPHandlingPolicy,
                             webrtc_ip_handling_policy_values[i]);
    blink::RendererPreferences renderer_preferences;
    renderer_preferences_util::UpdateFromSystemSettings(&renderer_preferences,
                                                        &profile_);
    EXPECT_EQ(renderer_preferences.webrtc_ip_handling_policy,
              expected_values[i]);
  }
}

TEST_F(RendererPreferencesUtilTest, WebRTCIPHandlingURLValidEntries) {
  base::Value::Dict valid_entry_1 =
      base::Value::Dict()
          .Set("url", "[*.]example.com")
          .Set("handling", blink::kWebRTCIPHandlingDisableNonProxiedUdp);
  base::Value::Dict no_url = base::Value::Dict().Set(
      "handling", blink::kWebRTCIPHandlingDisableNonProxiedUdp);
  base::Value::Dict invalid_url =
      base::Value::Dict().Set("url", "*.example.com");
  base::Value::Dict no_handling =
      base::Value::Dict().Set("url", "[*.]example.com");
  base::Value::Dict valid_entry_2 =
      base::Value::Dict()
          .Set("url", "*://google.com:*")
          .Set("handling", blink::kWebRTCIPHandlingDefault);

  base::Value::List list = base::Value::List()
                               .Append(std::move(valid_entry_1))
                               .Append(std::move(no_url))
                               .Append(std::move(invalid_url))
                               .Append(std::move(no_handling))
                               .Append(std::move(valid_entry_2));
  pref_service_->SetList(prefs::kWebRTCIPHandlingUrl, std::move(list));

  blink::RendererPreferences renderer_preferences;
  renderer_preferences_util::UpdateFromSystemSettings(&renderer_preferences,
                                                      &profile_);

  ASSERT_EQ(renderer_preferences.webrtc_ip_handling_urls.size(), 2u);

  EXPECT_EQ(renderer_preferences.webrtc_ip_handling_urls[0]
                .url_pattern.GetSchemeType(),
            ContentSettingsPattern::SCHEME_WILDCARD);
  EXPECT_EQ(
      renderer_preferences.webrtc_ip_handling_urls[0].url_pattern.GetHost(),
      "example.com");
  EXPECT_TRUE(renderer_preferences.webrtc_ip_handling_urls[0]
                  .url_pattern.HasDomainWildcard());
  EXPECT_EQ(
      renderer_preferences.webrtc_ip_handling_urls[0].url_pattern.GetScope(),
      ContentSettingsPattern::Scope::kWithDomainAndSchemeAndPortWildcard);
  EXPECT_EQ(renderer_preferences.webrtc_ip_handling_urls[0].handling,
            blink::mojom::WebRtcIpHandlingPolicy::kDisableNonProxiedUdp);

  EXPECT_EQ(renderer_preferences.webrtc_ip_handling_urls[1]
                .url_pattern.GetSchemeType(),
            ContentSettingsPattern::SCHEME_WILDCARD);
  EXPECT_EQ(
      renderer_preferences.webrtc_ip_handling_urls[1].url_pattern.GetHost(),
      "google.com");
  EXPECT_FALSE(renderer_preferences.webrtc_ip_handling_urls[1]
                   .url_pattern.HasDomainWildcard());
  EXPECT_EQ(
      renderer_preferences.webrtc_ip_handling_urls[1].url_pattern.GetScope(),
      ContentSettingsPattern::Scope::kWithSchemeAndPortWildcard);
  EXPECT_EQ(renderer_preferences.webrtc_ip_handling_urls[1].handling,
            blink::mojom::WebRtcIpHandlingPolicy::kDefault);
}
