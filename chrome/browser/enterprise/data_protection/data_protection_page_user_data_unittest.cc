// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_page_user_data.h"

#include "base/strings/stringprintf.h"
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

void AddDummyMatchedRule(safe_browsing::RTLookupResponse& rt_lookup_response,
                         const char* watermark_text,
                         bool allow_screenshot) {
  int count = rt_lookup_response.threat_info().size();
  auto* threat_info = rt_lookup_response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id(
      base::StringPrintf("test rule id-%d", count));
  matched_url_navigation_rule->set_rule_name(
      base::StringPrintf("test rule name-%d", count));
  matched_url_navigation_rule->set_matched_url_category(
      base::StringPrintf("test rule category-%d", count));
  matched_url_navigation_rule->mutable_watermark_message()
      ->set_watermark_message(watermark_text);
  matched_url_navigation_rule->set_block_screenshot(!allow_screenshot);
}

std::unique_ptr<safe_browsing::RTLookupResponse> BuildDummyResponse(
    const char* watermark_text,
    bool allow_screenshot) {
  auto rt_lookup_response = std::make_unique<safe_browsing::RTLookupResponse>();
  AddDummyMatchedRule(*rt_lookup_response, watermark_text, allow_screenshot);
  return rt_lookup_response;
}

}  // namespace

TEST_F(DataProtectionPageUserDataTest, TestCreateForPage) {
  auto rt_lookup_response = BuildDummyResponse("example", true);

  content::Page& page = web_contents_->GetPrimaryPage();
  content::PageUserData<
      enterprise_data_protection::DataProtectionPageUserData>::
      CreateForPage(page, std::string(), UrlSettings(),
                    std::move(rt_lookup_response));

  auto* ud =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_NE(ud->settings().watermark_text.find("example"), std::string::npos);
  ASSERT_TRUE(ud->rt_lookup_response());
  ASSERT_EQ(ud->rt_lookup_response()->threat_info_size(), 1);

  const auto& ud_threat_info = ud->rt_lookup_response()->threat_info(0);
  ASSERT_EQ(ud_threat_info.verdict_type(),
            safe_browsing::RTLookupResponse::ThreatInfo::WARN);

  const auto& ud_rule = ud_threat_info.matched_url_navigation_rule();
  ASSERT_EQ(ud_rule.rule_id(), "test rule id-0");
  ASSERT_EQ(ud_rule.rule_name(), "test rule name-0");
  ASSERT_EQ(ud_rule.matched_url_category(), "test rule category-0");
}

TEST_F(DataProtectionPageUserDataTest, NoRTURLLookupResponse) {
  content::Page& page = web_contents_->GetPrimaryPage();
  content::PageUserData<
      enterprise_data_protection::DataProtectionPageUserData>::
      CreateForPage(page, std::string(), UrlSettings(), nullptr);

  auto* ud =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_TRUE(ud->settings().watermark_text.empty());
}

TEST_F(DataProtectionPageUserDataTest, NoThreatInfo) {
  auto rt_lookup_response = std::make_unique<safe_browsing::RTLookupResponse>();

  content::Page& page = web_contents_->GetPrimaryPage();
  content::PageUserData<
      enterprise_data_protection::DataProtectionPageUserData>::
      CreateForPage(page, std::string(), UrlSettings(),
                    std::move(rt_lookup_response));

  auto* ud =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_TRUE(ud->settings().watermark_text.empty());
  ASSERT_TRUE(ud->rt_lookup_response());
  ASSERT_EQ(ud->rt_lookup_response()->threat_info_size(), 0);
}

TEST_F(DataProtectionPageUserDataTest, UpdateRTLookupResponse) {
  auto rt_lookup_response = BuildDummyResponse("first", true);

  content::Page& page = web_contents_->GetPrimaryPage();
  enterprise_data_protection::DataProtectionPageUserData::
      UpdateRTLookupResponse(page, std::string(),
                             std::move(rt_lookup_response));
  auto* ud =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_NE(ud->settings().watermark_text.find("first"), std::string::npos);

  rt_lookup_response = BuildDummyResponse("second", true);
  enterprise_data_protection::DataProtectionPageUserData::
      UpdateRTLookupResponse(page, std::string(),
                             std::move(rt_lookup_response));
  ud = enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_NE(ud->settings().watermark_text.find("second"), std::string::npos);
}

TEST_F(DataProtectionPageUserDataTest, UpdateScreenshotState) {
  content::Page& page = web_contents_->GetPrimaryPage();
  enterprise_data_protection::DataProtectionPageUserData::
      UpdateDataControlsScreenshotState(page, std::string(), false);
  auto* ud =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_FALSE(ud->settings().allow_screenshots);

  enterprise_data_protection::DataProtectionPageUserData::
      UpdateDataControlsScreenshotState(page, std::string(), true);
  ud = enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_TRUE(ud->settings().allow_screenshots);
}

TEST_F(DataProtectionPageUserDataTest, MergedScreenshotState) {
  content::Page& page = web_contents_->GetPrimaryPage();

  auto rt_lookup_response = BuildDummyResponse("first", false);
  enterprise_data_protection::DataProtectionPageUserData::
      UpdateRTLookupResponse(page, std::string(),
                             std::move(rt_lookup_response));
  enterprise_data_protection::DataProtectionPageUserData::
      UpdateDataControlsScreenshotState(page, std::string(), true);
  auto* ud =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_FALSE(ud->settings().allow_screenshots);

  rt_lookup_response = BuildDummyResponse("second", true);
  enterprise_data_protection::DataProtectionPageUserData::
      UpdateRTLookupResponse(page, std::string(),
                             std::move(rt_lookup_response));
  enterprise_data_protection::DataProtectionPageUserData::
      UpdateDataControlsScreenshotState(page, std::string(), false);
  ud = enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_FALSE(ud->settings().allow_screenshots);
}

TEST_F(DataProtectionPageUserDataTest,
       MultipleMatchedRulesWithDifferentSubactions) {
  auto rt_lookup_response = std::make_unique<safe_browsing::RTLookupResponse>();
  AddDummyMatchedRule(*rt_lookup_response, "example", true);
  AddDummyMatchedRule(*rt_lookup_response, "", false);

  content::Page& page = web_contents_->GetPrimaryPage();
  enterprise_data_protection::DataProtectionPageUserData::
      UpdateRTLookupResponse(page, std::string(),
                             std::move(rt_lookup_response));
  auto* ud =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_NE(ud->settings().watermark_text.find("example"), std::string::npos);
  ASSERT_FALSE(ud->settings().allow_screenshots);
}
}  // namespace enterprise_data_protection
