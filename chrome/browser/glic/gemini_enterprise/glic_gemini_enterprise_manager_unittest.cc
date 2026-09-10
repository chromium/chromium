// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/glic_gemini_enterprise_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/gemini_enterprise/gemini_enterprise.mojom.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

class GlicGeminiEnterpriseManagerUnitTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    manager_ = std::make_unique<GlicGeminiEnterpriseManager>(profile_.get());
    manager_->Bind(handler_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    manager_.reset();
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<GlicGeminiEnterpriseManager> manager_;
  mojo::Remote<mojom::GeminiEnterpriseHandler> handler_remote_;
};

TEST_F(GlicGeminiEnterpriseManagerUnitTest,
       IsSignInURLAllowedValidatesSchemesAndOrigins) {
  // Valid HTTPS Gaia origin:
  EXPECT_TRUE(manager_->IsSignInURLAllowedForTesting(
      GURL("https://accounts.google.com/signin")));
  EXPECT_TRUE(manager_->IsSignInURLAllowedForTesting(
      GURL("https://accounts.google.com/o/oauth2/auth")));

  // Disallowed HTTP scheme (must be HTTPS):
  EXPECT_FALSE(manager_->IsSignInURLAllowedForTesting(
      GURL("http://accounts.google.com/signin")));

  // Disallowed non-Gaia, non-guest HTTPS origin:
  EXPECT_FALSE(
      manager_->IsSignInURLAllowedForTesting(GURL("https://evil.com/signin")));
  EXPECT_FALSE(manager_->IsSignInURLAllowedForTesting(
      GURL("https://example.com/login")));

  // Disallowed URLs with credentials (userinfo):
  EXPECT_FALSE(manager_->IsSignInURLAllowedForTesting(
      GURL("https://user:pass@accounts.google.com/signin")));
  EXPECT_FALSE(manager_->IsSignInURLAllowedForTesting(
      GURL("https://user@accounts.google.com/signin")));

  // Disallowed URLs exceeding maximum allowed length (64KB):
  std::string long_query(65536, 'a');
  EXPECT_FALSE(manager_->IsSignInURLAllowedForTesting(
      GURL("https://accounts.google.com/signin?" + long_query)));

  // Disallowed internal and script schemes:
  EXPECT_FALSE(
      manager_->IsSignInURLAllowedForTesting(GURL("chrome://settings")));
  EXPECT_FALSE(
      manager_->IsSignInURLAllowedForTesting(GURL("javascript:alert(1)")));
  EXPECT_FALSE(
      manager_->IsSignInURLAllowedForTesting(GURL("file:///etc/passwd")));

  // Invalid URL:
  EXPECT_FALSE(manager_->IsSignInURLAllowedForTesting(GURL("")));
  EXPECT_FALSE(manager_->IsSignInURLAllowedForTesting(GURL("not-a-valid-url")));
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest, AllowsConfiguredGuestOrigin) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kGlicGuestURL,
      "https://localhost.corp.google.com:10443/side-panel");
  GlicGeminiEnterpriseManager manager(profile_.get());

  EXPECT_TRUE(manager.IsSignInURLAllowedForTesting(
      GURL("https://localhost.corp.google.com:10443/auth/signin")));
  EXPECT_FALSE(manager.IsSignInURLAllowedForTesting(
      GURL("https://otherhost.corp.google.com:10443/auth/signin")));
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest,
       CloseSignInTabReturnsNoSignInTabWhenNeverOpened) {
  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  handler_remote_->CloseSignInTab(nullptr, close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kNoSignInTab);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest, RejectsMissingSignInUrl) {
  mojo::Remote<mojom::GeminiEnterpriseHandler> remote;
  manager_->Bind(remote.BindNewPipeAndPassReceiver());

  {
    base::test::TestFuture<mojom::OpenSignInTabResult> future;
    remote->OpenSignInTab(nullptr, future.GetCallback());
    EXPECT_EQ(future.Take(), mojom::OpenSignInTabResult::kErrorNoUrl);
  }

  {
    base::test::TestFuture<mojom::OpenSignInTabResult> future;
    auto options = mojom::OpenSignInTabOptions::New();
    remote->OpenSignInTab(std::move(options), future.GetCallback());
    EXPECT_EQ(future.Take(), mojom::OpenSignInTabResult::kErrorNoUrl);
  }
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest, RejectsOffTheRecordProfile) {
  TestingProfile::Builder otr_builder;
  Profile* otr_profile = otr_builder.BuildIncognito(profile_.get());
  GlicGeminiEnterpriseManager otr_manager(otr_profile);
  mojo::Remote<mojom::GeminiEnterpriseHandler> otr_remote;
  otr_manager.Bind(otr_remote.BindNewPipeAndPassReceiver());

  // Calling OpenSignInTab on OTR manager should immediately fail.
  base::test::TestFuture<mojom::OpenSignInTabResult> open_future;
  auto options = mojom::OpenSignInTabOptions::New();
  options->signin_url = GURL("https://accounts.google.com/signin");
  otr_remote->OpenSignInTab(std::move(options), open_future.GetCallback());
  EXPECT_EQ(open_future.Take(), mojom::OpenSignInTabResult::kErrorFailure);

  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  otr_remote->CloseSignInTab(nullptr, close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kNoSignInTab);
}

}  // namespace
}  // namespace glic
