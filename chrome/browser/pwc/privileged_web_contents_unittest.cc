// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/privileged_web_contents.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace pwc {
namespace {

std::unique_ptr<FixedPwcPolicyDelegate> MakeTestDelegate() {
  return std::make_unique<FixedPwcPolicyDelegate>(
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))},
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))});
}

class PrivilegedWebContentsTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        mojom::features::kPrivilegedWebContents);
    ChromeRenderViewHostTestHarness::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivilegedWebContentsTest, CreateOwnsAWebContents) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  ASSERT_TRUE(pwc);
  ASSERT_TRUE(pwc->web_contents());
  EXPECT_EQ(pwc->web_contents()->GetBrowserContext(), profile());
  EXPECT_EQ(pwc->component(), PrivilegedComponent::kTestComponent);
  EXPECT_EQ(pwc->policy().component(), PrivilegedComponent::kTestComponent);
  EXPECT_EQ(pwc->web_contents()->GetDelegate(), pwc.get());
}

TEST_F(PrivilegedWebContentsTest, UsesDefaultStoragePartition) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  content::RenderProcessHost* process =
      pwc->web_contents()->GetPrimaryMainFrame()->GetProcess();
  EXPECT_EQ(process->GetStoragePartition(),
            profile()->GetDefaultStoragePartition());
}

TEST_F(PrivilegedWebContentsTest, FromWebContentsRoundTrips) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(pwc->web_contents()),
            pwc.get());
}

TEST_F(PrivilegedWebContentsTest, FromWebContentsIsNullForOrdinaryContents) {
  // The harness's own WebContents is not owned by a PrivilegedWebContents.
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(web_contents()), nullptr);
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(nullptr), nullptr);
}

TEST_F(PrivilegedWebContentsTest, DestructionIsClean) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  pwc.reset();
  // No crash, and unrelated WebContents are unaffected.
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(web_contents()), nullptr);
}

}  // namespace
}  // namespace pwc
