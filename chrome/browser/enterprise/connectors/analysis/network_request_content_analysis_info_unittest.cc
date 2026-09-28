// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/network_request_content_analysis_info.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/network_request_analysis_request.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kTabUrl[] = "https://foo.com/page";
constexpr char16_t kTabTitle[] = u"Tab title";
constexpr char kRequestUrl[] = "https://bar.org/upload";
constexpr char kChildFrameUrl[] = "https://child.com/frame";
constexpr char kEmail[] = "user@example.com";
constexpr char kDmToken[] = "dm_token";

AnalysisSettings CloudSettings() {
  CloudAnalysisSettings cloud_settings;
  cloud_settings.analysis_url = GURL("https://scan.com/");
  cloud_settings.dm_token = kDmToken;

  AnalysisSettings settings;
  settings.cloud_or_local_settings =
      CloudOrLocalAnalysisSettings(std::move(cloud_settings));
  settings.tags = {{"dlp", TagSettings()}};
  settings.block_until_verdict = BlockUntilVerdict::kNoBlock;
  settings.per_profile = true;
  return settings;
}

class NetworkRequestContentAnalysisInfoTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(kEmail, signin::ConsentLevel::kSignin);

    NavigateAndCommit(GURL(kTabUrl));
    content::WebContentsTester::For(web_contents())->SetTitle(kTabTitle);
  }

  void TearDown() override {
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  std::unique_ptr<NetworkRequestContentAnalysisInfo> CreateInfo(
      std::optional<content::GlobalRenderFrameHostId> initiator_frame_id =
          std::nullopt) {
    return std::make_unique<NetworkRequestContentAnalysisInfo>(
        CloudSettings(), web_contents(), GURL(kRequestUrl),
        std::move(initiator_frame_id));
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

}  // namespace

TEST_F(NetworkRequestContentAnalysisInfoTest, Getters) {
  auto info = CreateInfo(main_rfh()->GetGlobalId());

  EXPECT_EQ(1u, info->settings().tags.size());
  EXPECT_EQ(1u, info->settings().tags.count("dlp"));
  EXPECT_EQ(kDmToken, info->settings().cloud_or_local_settings.dm_token());
  EXPECT_EQ(IdentityManagerFactory::GetForProfile(profile()),
            info->identity_manager());
  EXPECT_EQ(1, info->user_action_requests_count());
  EXPECT_EQ("Tab title", info->tab_title());
  EXPECT_EQ(kEmail, info->email());
  EXPECT_EQ(GURL(kRequestUrl), info->url());
  EXPECT_EQ(GURL(kTabUrl), info->tab_url());
  EXPECT_EQ(ContentAnalysisRequest::UNKNOWN, info->reason());
  EXPECT_EQ(web_contents(), info->web_contents());

  // The request is made from the main frame, so there are no iframe URLs.
  EXPECT_TRUE(info->frame_url_chain().empty());

  // The user action ID is 128 random bytes encoded as hex, so each network
  // request should get its own ID.
  EXPECT_EQ(256u, info->user_action_id().size());
  EXPECT_NE(info->user_action_id(), CreateInfo()->user_action_id());
}

TEST_F(NetworkRequestContentAnalysisInfoTest, ChildFrameInitiator) {
  content::RenderFrameHost* child_frame =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  child_frame = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kChildFrameUrl), child_frame);
  ASSERT_TRUE(child_frame);

  auto info = CreateInfo(child_frame->GetGlobalId());

  // The tab URL is always the URL of the main frame, and the frame URL chain
  // starts at the frame that made the request.
  EXPECT_EQ(GURL(kTabUrl), info->tab_url());
  ASSERT_EQ(1, info->frame_url_chain().size());
  EXPECT_EQ(kChildFrameUrl, info->frame_url_chain()[0]);
}

TEST_F(NetworkRequestContentAnalysisInfoTest, InitializeRequest) {
  auto info = CreateInfo();

  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendCopyOfBytes(base::as_byte_span(std::string_view("data")));
  NetworkRequestAnalysisRequest request(
      info->settings().cloud_or_local_settings.cloud_settings(),
      std::move(body), base::DoNothing(),
      /*policy_connector_getter=*/base::NullCallback());
  info->InitializeRequest(&request, /*include_enterprise_only_fields=*/true);

  const ContentAnalysisRequest& proto = request.content_analysis_request();
  EXPECT_EQ(AnalysisConnector::NETWORK_REQUEST, proto.analysis_connector());
  EXPECT_EQ(kRequestUrl, proto.request_data().url());
  EXPECT_EQ(kTabUrl, proto.request_data().tab_url());
  EXPECT_EQ(kEmail, proto.request_data().email());
  EXPECT_EQ(kDmToken, proto.device_token());
  EXPECT_EQ(info->user_action_id(), proto.user_action_id());
  EXPECT_EQ(1, proto.user_action_requests_count());
  ASSERT_EQ(1, proto.tags_size());
  EXPECT_EQ("dlp", proto.tags(0));
  EXPECT_FALSE(proto.blocking());
  EXPECT_FALSE(proto.has_reason());
  EXPECT_TRUE(request.per_profile_request());
}

TEST_F(NetworkRequestContentAnalysisInfoTest, OutlivesWebContents) {
  auto info = CreateInfo(main_rfh()->GetGlobalId());
  std::string user_action_id = info->user_action_id();

  DeleteContents();

  // Values computed at construction should still be available once the tab
  // is gone.
  EXPECT_FALSE(info->web_contents());
  EXPECT_EQ(GURL(kRequestUrl), info->url());
  EXPECT_EQ(GURL(kTabUrl), info->tab_url());
  EXPECT_EQ("Tab title", info->tab_title());
  EXPECT_EQ(kEmail, info->email());
  EXPECT_EQ(user_action_id, info->user_action_id());
  EXPECT_EQ(1u, info->settings().tags.count("dlp"));
  EXPECT_EQ(std::string(), info->GetContentAreaAccountEmail());
}

}  // namespace enterprise_connectors
