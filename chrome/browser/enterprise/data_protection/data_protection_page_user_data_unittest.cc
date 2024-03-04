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

TEST_F(DataProtectionPageUserDataTest, TestCreatePopulatesWatermarkString) {
  content::Page& page = web_contents_->GetPrimaryPage();
  content::PageUserData<
      enterprise_data_protection::DataProtectionPageUserData>::
      CreateForPage(page, "example");
  auto* ud =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(page);
  ASSERT_EQ(ud->watermark_text(), "example");
}

}  // namespace enterprise_data_protection
