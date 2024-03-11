// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_page_user_data.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"

namespace enterprise_data_protection {

namespace {

// RenderViewHostTestHarness might not be necessary, but the Page constructor is
// private, so I looked at other tests and found that this is how it's created.
class DataProtectionPageUserDataTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    web_contents_ = CreateTestWebContents();
  }

  void TearDown() override {
    web_contents_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
};

}  // namespace

TEST_F(DataProtectionPageUserDataTest, TestCreateForPage) {
  auto rt_lookup_response = std::make_unique<safe_browsing::RTLookupResponse>();
  auto* threat_info = rt_lookup_response->add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("test rule id");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");

  content::Page& page = web_contents_->GetPrimaryPage();
  content::PageUserData<
      enterprise_data_protection::DataProtectionPageUserData>::
      CreateForPage(page, "example", std::move(rt_lookup_response));

  auto* ud =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_EQ(ud->watermark_text(), "example");
  ASSERT_TRUE(ud->rt_lookup_response());
  ASSERT_EQ(ud->rt_lookup_response()->threat_info_size(), 1);

  const auto& ud_threat_info = ud->rt_lookup_response()->threat_info(0);
  ASSERT_EQ(ud_threat_info.verdict_type(),
            safe_browsing::RTLookupResponse::ThreatInfo::WARN);

  const auto& ud_rule = ud_threat_info.matched_url_navigation_rule();
  ASSERT_EQ(ud_rule.rule_id(), "test rule id");
  ASSERT_EQ(ud_rule.rule_name(), "test rule name");
  ASSERT_EQ(ud_rule.matched_url_category(), "test rule category");
}

}  // namespace enterprise_data_protection
