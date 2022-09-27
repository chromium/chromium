// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_client_hints_controller_delegate.h"

#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {

class AwClientHintsControllerDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    client_hints_controller_delegate_ =
        std::make_unique<AwClientHintsControllerDelegate>();
  }

  std::unique_ptr<content::ClientHintsControllerDelegate>
      client_hints_controller_delegate_;
};

TEST_F(AwClientHintsControllerDelegateTest, GetNetworkQualityTracker) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  EXPECT_EQ(nullptr,
            client_hints_controller_delegate_->GetNetworkQualityTracker());
}

TEST_F(AwClientHintsControllerDelegateTest, GetAllowedClientHintsFromSource) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  client_hints_controller_delegate_->GetAllowedClientHintsFromSource(
      url::Origin(), nullptr);
}

TEST_F(AwClientHintsControllerDelegateTest, IsJavaScriptAllowed) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  EXPECT_FALSE(client_hints_controller_delegate_->IsJavaScriptAllowed(GURL(""),
                                                                      nullptr));
}

TEST_F(AwClientHintsControllerDelegateTest, AreThirdPartyCookiesBlocked) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  EXPECT_FALSE(
      client_hints_controller_delegate_->AreThirdPartyCookiesBlocked(GURL("")));
}

TEST_F(AwClientHintsControllerDelegateTest, GetUserAgentMetadata) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  EXPECT_EQ(blink::UserAgentMetadata(),
            client_hints_controller_delegate_->GetUserAgentMetadata());
}

TEST_F(AwClientHintsControllerDelegateTest, PersistClientHints) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  client_hints_controller_delegate_->PersistClientHints(url::Origin(), nullptr,
                                                        {});
}

TEST_F(AwClientHintsControllerDelegateTest, SetAdditionalClientHints) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  client_hints_controller_delegate_->SetAdditionalClientHints({});
}

TEST_F(AwClientHintsControllerDelegateTest, ClearAdditionalClientHints) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  client_hints_controller_delegate_->ClearAdditionalClientHints();
}

TEST_F(AwClientHintsControllerDelegateTest,
       SetMostRecentMainFrameViewportSize) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  client_hints_controller_delegate_->SetMostRecentMainFrameViewportSize(
      gfx::Size(0, 0));
}

TEST_F(AwClientHintsControllerDelegateTest,
       GetMostRecentMainFrameViewportSize) {
  // TODO(crbug.com/921655): Actually test function once implemented.
  EXPECT_EQ(
      gfx::Size(0, 0),
      client_hints_controller_delegate_->GetMostRecentMainFrameViewportSize());
}

}  // namespace android_webview
