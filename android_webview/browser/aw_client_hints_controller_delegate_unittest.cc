// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_client_hints_controller_delegate.h"

#include "base/memory/ref_counted.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {

namespace {
class PermissiveAwClientHintsControllerDelegate
    : public AwClientHintsControllerDelegate {
 public:
  explicit PermissiveAwClientHintsControllerDelegate(PrefService* pref_service)
      : AwClientHintsControllerDelegate(pref_service) {}

  bool IsJavaScriptAllowed(const GURL& url,
                           content::RenderFrameHost* parent_rfh) override {
    // We need to be permissive here to test persisting client hints.
    return true;
  }
};
}  // namespace

class AwClientHintsControllerDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterDictionaryPref(
        prefs::kClientHintsCachedPerOriginMap);
    client_hints_controller_delegate_ =
        std::make_unique<AwClientHintsControllerDelegate>(prefs_.get());
    permissive_client_hints_controller_delegate_ =
        std::make_unique<PermissiveAwClientHintsControllerDelegate>(
            prefs_.get());
  }

  std::unique_ptr<content::ClientHintsControllerDelegate>
      client_hints_controller_delegate_;
  std::unique_ptr<content::ClientHintsControllerDelegate>
      permissive_client_hints_controller_delegate_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

TEST_F(AwClientHintsControllerDelegateTest, GetNetworkQualityTracker) {
  EXPECT_EQ(nullptr,
            client_hints_controller_delegate_->GetNetworkQualityTracker());
}

TEST_F(AwClientHintsControllerDelegateTest, GetAllowedClientHintsFromSource) {
  // Verify empty Origin is rejected even if additional hints are set.
  blink::EnabledClientHints enabled_hints;
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin(), &enabled_hints);
  EXPECT_TRUE(enabled_hints.GetEnabledHints().empty());
  permissive_client_hints_controller_delegate_->SetAdditionalClientHints(
      {network::mojom::WebClientHintsType::kDeviceMemory});
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin(), &enabled_hints);
  EXPECT_TRUE(enabled_hints.GetEnabledHints().empty());
  permissive_client_hints_controller_delegate_->ClearAdditionalClientHints();

  // Verify insecure Origin is rejected even if additional hints are set.
  enabled_hints = blink::EnabledClientHints();
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin::Create(GURL("https://example.com")), &enabled_hints);
  EXPECT_TRUE(enabled_hints.GetEnabledHints().empty());
  permissive_client_hints_controller_delegate_->SetAdditionalClientHints(
      {network::mojom::WebClientHintsType::kDeviceMemory});
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin::Create(GURL("http://example.com")), &enabled_hints);
  EXPECT_TRUE(enabled_hints.GetEnabledHints().empty());
  permissive_client_hints_controller_delegate_->ClearAdditionalClientHints();

  // Verify persisted and additional hints are combined.
  enabled_hints = blink::EnabledClientHints();
  permissive_client_hints_controller_delegate_->SetAdditionalClientHints(
      {network::mojom::WebClientHintsType::kDeviceMemory});
  permissive_client_hints_controller_delegate_->PersistClientHints(
      url::Origin::Create(GURL("https://example.com")), nullptr,
      {network::mojom::WebClientHintsType::kPrefersColorScheme});
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin::Create(GURL("https://example.com")), &enabled_hints);
  EXPECT_EQ(
      enabled_hints.GetEnabledHints(),
      std::vector({network::mojom::WebClientHintsType::kPrefersColorScheme,
                   network::mojom::WebClientHintsType::kDeviceMemory}));
}

TEST_F(AwClientHintsControllerDelegateTest, IsJavaScriptAllowed) {
  EXPECT_TRUE(client_hints_controller_delegate_->IsJavaScriptAllowed(GURL(""),
                                                                     nullptr));
  EXPECT_TRUE(client_hints_controller_delegate_->IsJavaScriptAllowed(
      GURL("https://example.com/"), nullptr));
}

TEST_F(AwClientHintsControllerDelegateTest, AreThirdPartyCookiesBlocked) {
  EXPECT_FALSE(client_hints_controller_delegate_->AreThirdPartyCookiesBlocked(
      GURL(""), nullptr));
  EXPECT_FALSE(client_hints_controller_delegate_->AreThirdPartyCookiesBlocked(
      GURL("https://example.com"), nullptr));
}

TEST_F(AwClientHintsControllerDelegateTest, GetUserAgentMetadata) {
  EXPECT_EQ(embedder_support::GetUserAgentMetadata(prefs_.get()),
            client_hints_controller_delegate_->GetUserAgentMetadata());
}

TEST_F(AwClientHintsControllerDelegateTest, PersistClientHints) {
  // Empty origin can't persist hints.
  blink::EnabledClientHints enabled_hints;
  permissive_client_hints_controller_delegate_->PersistClientHints(
      url::Origin(), nullptr,
      {network::mojom::WebClientHintsType::kDeviceMemory});
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin(), &enabled_hints);
  EXPECT_TRUE(enabled_hints.GetEnabledHints().empty());

  // Insecure origin can't persist hints.
  enabled_hints = blink::EnabledClientHints();
  permissive_client_hints_controller_delegate_->PersistClientHints(
      url::Origin::Create(GURL("http://example.com")), nullptr,
      {network::mojom::WebClientHintsType::kDeviceMemory});
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin::Create(GURL("http://example.com")), &enabled_hints);
  EXPECT_TRUE(enabled_hints.GetEnabledHints().empty());

  // Persisting hints for one origin doesnt affect others.
  enabled_hints = blink::EnabledClientHints();
  permissive_client_hints_controller_delegate_->PersistClientHints(
      url::Origin::Create(GURL("https://example.com")), nullptr,
      {network::mojom::WebClientHintsType::kDeviceMemory});
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin::Create(GURL("https://example.com")), &enabled_hints);
  EXPECT_EQ(enabled_hints.GetEnabledHints(),
            std::vector({network::mojom::WebClientHintsType::kDeviceMemory}));
  enabled_hints = blink::EnabledClientHints();
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin::Create(GURL("https://another.example.com")), &enabled_hints);
  EXPECT_TRUE(enabled_hints.GetEnabledHints().empty());

  // Persisted hints can be cleared.
  enabled_hints = blink::EnabledClientHints();
  permissive_client_hints_controller_delegate_->PersistClientHints(
      url::Origin::Create(GURL("https://example.com")), nullptr,
      {network::mojom::WebClientHintsType::kDeviceMemory});
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin::Create(GURL("https://example.com")), &enabled_hints);
  EXPECT_EQ(enabled_hints.GetEnabledHints(),
            std::vector({network::mojom::WebClientHintsType::kDeviceMemory}));
  enabled_hints = blink::EnabledClientHints();
  permissive_client_hints_controller_delegate_->PersistClientHints(
      url::Origin::Create(GURL("https://example.com")), nullptr, {});
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin::Create(GURL("https://example.com")), &enabled_hints);
  EXPECT_TRUE(enabled_hints.GetEnabledHints().empty());
}

TEST_F(AwClientHintsControllerDelegateTest, SetAdditionalClientHints) {
  blink::EnabledClientHints enabled_hints;
  permissive_client_hints_controller_delegate_->SetAdditionalClientHints(
      {network::mojom::WebClientHintsType::kDeviceMemory});
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin::Create(GURL("https://example.com")), &enabled_hints);
  EXPECT_EQ(enabled_hints.GetEnabledHints(),
            std::vector({network::mojom::WebClientHintsType::kDeviceMemory}));
}

TEST_F(AwClientHintsControllerDelegateTest, ClearAdditionalClientHints) {
  blink::EnabledClientHints enabled_hints;
  permissive_client_hints_controller_delegate_->SetAdditionalClientHints(
      {network::mojom::WebClientHintsType::kDeviceMemory});
  permissive_client_hints_controller_delegate_->ClearAdditionalClientHints();
  permissive_client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin::Create(GURL("https://example.com")), &enabled_hints);
  EXPECT_TRUE(enabled_hints.GetEnabledHints().empty());
}

TEST_F(AwClientHintsControllerDelegateTest,
       SetMostRecentMainFrameViewportSize) {
  client_hints_controller_delegate_->SetMostRecentMainFrameViewportSize(
      gfx::Size(1, 1));
  EXPECT_EQ(
      gfx::Size(1, 1),
      client_hints_controller_delegate_->GetMostRecentMainFrameViewportSize());
}

TEST_F(AwClientHintsControllerDelegateTest,
       GetMostRecentMainFrameViewportSize) {
  EXPECT_EQ(
      gfx::Size(0, 0),
      client_hints_controller_delegate_->GetMostRecentMainFrameViewportSize());
}

}  // namespace android_webview
