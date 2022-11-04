// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_client_hints_controller_delegate.h"

#include "base/memory/ref_counted.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {

class AwClientHintsControllerDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    client_hints_controller_delegate_ =
        std::make_unique<AwClientHintsControllerDelegate>(prefs_.get());
  }

  std::unique_ptr<content::ClientHintsControllerDelegate>
      client_hints_controller_delegate_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

TEST_F(AwClientHintsControllerDelegateTest, GetNetworkQualityTracker) {
  EXPECT_EQ(nullptr,
            client_hints_controller_delegate_->GetNetworkQualityTracker());
}

TEST_F(AwClientHintsControllerDelegateTest, GetAllowedClientHintsFromSource) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin(), nullptr);
}

TEST_F(AwClientHintsControllerDelegateTest, IsJavaScriptAllowed) {
  EXPECT_FALSE(client_hints_controller_delegate_->IsJavaScriptAllowed(GURL(""),
                                                                      nullptr));
  EXPECT_FALSE(client_hints_controller_delegate_->IsJavaScriptAllowed(
      GURL("https://example.com/"), nullptr));
  // TODO(crbug.com/921655): Add integration test when the rest is implemented.
}

TEST_F(AwClientHintsControllerDelegateTest, AreThirdPartyCookiesBlocked) {
  EXPECT_TRUE(client_hints_controller_delegate_->AreThirdPartyCookiesBlocked(
      GURL(""), nullptr));
  EXPECT_TRUE(client_hints_controller_delegate_->AreThirdPartyCookiesBlocked(
      GURL("https://example.com"), nullptr));
  // TODO(crbug.com/921655): Add integration test when the rest is implemented.
}

TEST_F(AwClientHintsControllerDelegateTest, GetUserAgentMetadata) {
  EXPECT_EQ(embedder_support::GetUserAgentMetadata(prefs_.get()),
            client_hints_controller_delegate_->GetUserAgentMetadata());
}

TEST_F(AwClientHintsControllerDelegateTest, PersistClientHints) {
  client_hints_controller_delegate_->PersistClientHints(url::Origin(), nullptr,
                                                        {});
  client_hints_controller_delegate_->PersistClientHints(
      url::Origin::Create(GURL("https://example.com")), nullptr, {});
  client_hints_controller_delegate_->PersistClientHints(
      url::Origin(), nullptr,
      {network::mojom::WebClientHintsType::kDeviceMemory});
  client_hints_controller_delegate_->PersistClientHints(
      url::Origin::Create(GURL("https://example.com")), nullptr,
      {network::mojom::WebClientHintsType::kDeviceMemory});
  // TODO(crbug.com/921655): Test with GetAllowedClientHintsFromSource
}

TEST_F(AwClientHintsControllerDelegateTest, SetAdditionalClientHints) {
  client_hints_controller_delegate_->SetAdditionalClientHints(
      {network::mojom::WebClientHintsType::kDeviceMemory});
  // TODO(crbug.com/921655): Test with GetAllowedClientHintsFromSource
}

TEST_F(AwClientHintsControllerDelegateTest, ClearAdditionalClientHints) {
  client_hints_controller_delegate_->SetAdditionalClientHints(
      {network::mojom::WebClientHintsType::kDeviceMemory});
  client_hints_controller_delegate_->ClearAdditionalClientHints();
  // TODO(crbug.com/921655): Test with GetAllowedClientHintsFromSource
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
