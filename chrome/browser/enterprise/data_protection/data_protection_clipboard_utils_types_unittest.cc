// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils_types.h"

#include <optional>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace enterprise_data_protection {

namespace {

class DataProtectionClipboardUtilsTypesTest : public testing::Test {
 public:
  DataProtectionClipboardUtilsTypesTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile1_ = profile_manager_.CreateTestingProfile("test-user-1");
    profile2_ = profile_manager_.CreateTestingProfile("test-user-2");
  }

  content::WebContents* web_contents() {
    if (!web_contents_) {
      web_contents_ =
          content::WebContentsTester::CreateTestWebContents(profile1_, nullptr);
    }
    return web_contents_.get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile1_;
  raw_ptr<TestingProfile> profile2_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(DataProtectionClipboardUtilsTypesTest, BasicPasteSource_DefaultValues) {
  BasicPasteSource source;
  EXPECT_FALSE(source.data_transfer_endpoint.has_value());
  EXPECT_EQ(source.browser_context.get(), nullptr);
  EXPECT_FALSE(source.gemini_in_chrome);
  EXPECT_TRUE(source.title.empty());
  EXPECT_TRUE(source.page_content_type.empty());
  EXPECT_EQ(source.url(), GURL());
  EXPECT_EQ(source, BasicPasteSource());
}

TEST_F(DataProtectionClipboardUtilsTypesTest, BasicPasteSource_Url) {
  BasicPasteSource source;
  EXPECT_EQ(source.url(), GURL());

  // Non-URL endpoint type should return empty GURL.
  source.data_transfer_endpoint =
      ui::DataTransferEndpoint(ui::EndpointType::kClipboardHistory);
  EXPECT_EQ(source.url(), GURL());

  source.data_transfer_endpoint =
      ui::DataTransferEndpoint(ui::EndpointType::kDefault);
  EXPECT_EQ(source.url(), GURL());

  // URL endpoint type should return the underlying URL.
  const GURL test_url("https://example.com/test");
  source.data_transfer_endpoint = ui::DataTransferEndpoint(test_url);
  EXPECT_EQ(source.url(), test_url);
}

TEST_F(DataProtectionClipboardUtilsTypesTest,
       BasicPasteSource_CopyAndAssignment) {
  BasicPasteSource src;
  src.data_transfer_endpoint =
      ui::DataTransferEndpoint(GURL("https://example.com"));
  src.browser_context = profile1_->GetWeakPtr();
  src.gemini_in_chrome = true;
  src.title = "Page Title";
  src.page_content_type = "text/html";

  // Copy construction.
  BasicPasteSource copy_constructed(src);
  EXPECT_EQ(src, copy_constructed);
  EXPECT_EQ(copy_constructed.url(), GURL("https://example.com"));

  // Copy assignment.
  BasicPasteSource copy_assigned;
  copy_assigned = src;
  EXPECT_EQ(src, copy_assigned);

  // Move construction.
  BasicPasteSource to_move(src);
  BasicPasteSource move_constructed(std::move(to_move));
  EXPECT_EQ(src, move_constructed);

  // Move assignment.
  BasicPasteSource to_move_assign(src);
  BasicPasteSource move_assigned;
  move_assigned = std::move(to_move_assign);
  EXPECT_EQ(src, move_assigned);
}

TEST_F(DataProtectionClipboardUtilsTypesTest,
       BasicPasteSource_EqualityAndInequality) {
  BasicPasteSource base_source;
  base_source.data_transfer_endpoint =
      ui::DataTransferEndpoint(GURL("https://example.com"));
  base_source.browser_context = profile1_->GetWeakPtr();
  base_source.gemini_in_chrome = true;
  base_source.title = "Page Title";
  base_source.page_content_type = "text/html";

  // Reflexive equality.
  EXPECT_EQ(base_source, base_source);

  // Inequality: data_transfer_endpoint.
  {
    BasicPasteSource modified = base_source;
    modified.data_transfer_endpoint = std::nullopt;
    EXPECT_NE(base_source, modified);

    modified.data_transfer_endpoint =
        ui::DataTransferEndpoint(GURL("https://different.com"));
    EXPECT_NE(base_source, modified);

    modified.data_transfer_endpoint =
        ui::DataTransferEndpoint(ui::EndpointType::kClipboardHistory);
    EXPECT_NE(base_source, modified);
  }

  // Inequality: browser_context.
  {
    BasicPasteSource modified = base_source;
    modified.browser_context = nullptr;
    EXPECT_NE(base_source, modified);

    modified.browser_context = profile2_->GetWeakPtr();
    EXPECT_NE(base_source, modified);
  }

  // Inequality: gemini_in_chrome.
  {
    BasicPasteSource modified = base_source;
    modified.gemini_in_chrome = false;
    EXPECT_NE(base_source, modified);
  }

  // Inequality: title.
  {
    BasicPasteSource modified = base_source;
    modified.title = "Different Title";
    EXPECT_NE(base_source, modified);

    modified.title = "";
    EXPECT_NE(base_source, modified);
  }

  // Inequality: page_content_type.
  {
    BasicPasteSource modified = base_source;
    modified.page_content_type = "application/pdf";
    EXPECT_NE(base_source, modified);

    modified.page_content_type = "";
    EXPECT_NE(base_source, modified);
  }
}

TEST_F(DataProtectionClipboardUtilsTypesTest, FullPasteSource_DefaultValues) {
  FullPasteSource source;
  EXPECT_FALSE(source.data_transfer_endpoint.has_value());
  EXPECT_EQ(source.browser_context.get(), nullptr);
  EXPECT_FALSE(source.gemini_in_chrome);
  EXPECT_TRUE(source.title.empty());
  EXPECT_TRUE(source.page_content_type.empty());
  EXPECT_TRUE(source.active_user.empty());
  EXPECT_EQ(source.url(), GURL());
  EXPECT_EQ(source, FullPasteSource());
}

TEST_F(DataProtectionClipboardUtilsTypesTest,
       FullPasteSource_CopyAndAssignment) {
  FullPasteSource src;
  src.data_transfer_endpoint =
      ui::DataTransferEndpoint(GURL("https://example.com"));
  src.browser_context = profile1_->GetWeakPtr();
  src.gemini_in_chrome = true;
  src.title = "Page Title";
  src.page_content_type = "text/html";
  src.active_user = "user@example.com";

  // Copy construction.
  FullPasteSource copy_constructed(src);
  EXPECT_EQ(src, copy_constructed);
  EXPECT_EQ(copy_constructed.active_user, "user@example.com");

  // Copy assignment.
  FullPasteSource copy_assigned;
  copy_assigned = src;
  EXPECT_EQ(src, copy_assigned);

  // Move construction.
  FullPasteSource to_move(src);
  FullPasteSource move_constructed(std::move(to_move));
  EXPECT_EQ(src, move_constructed);

  // Move assignment.
  FullPasteSource to_move_assign(src);
  FullPasteSource move_assigned;
  move_assigned = std::move(to_move_assign);
  EXPECT_EQ(src, move_assigned);
}

TEST_F(DataProtectionClipboardUtilsTypesTest,
       FullPasteSource_EqualityAndInequality) {
  FullPasteSource base_source;
  base_source.data_transfer_endpoint =
      ui::DataTransferEndpoint(GURL("https://example.com"));
  base_source.browser_context = profile1_->GetWeakPtr();
  base_source.gemini_in_chrome = true;
  base_source.title = "Page Title";
  base_source.page_content_type = "text/html";
  base_source.active_user = "user@example.com";

  // Reflexive equality.
  EXPECT_EQ(base_source, base_source);

  // Base fields differences lead to inequality.
  {
    FullPasteSource modified = base_source;
    modified.data_transfer_endpoint = std::nullopt;
    EXPECT_NE(base_source, modified);
  }
  {
    FullPasteSource modified = base_source;
    modified.browser_context = profile2_->GetWeakPtr();
    EXPECT_NE(base_source, modified);
  }
  {
    FullPasteSource modified = base_source;
    modified.gemini_in_chrome = false;
    EXPECT_NE(base_source, modified);
  }
  {
    FullPasteSource modified = base_source;
    modified.title = "Different Title";
    EXPECT_NE(base_source, modified);
  }
  {
    FullPasteSource modified = base_source;
    modified.page_content_type = "application/pdf";
    EXPECT_NE(base_source, modified);
  }

  // active_user inequality.
  {
    FullPasteSource modified = base_source;
    modified.active_user = "other_user@example.com";
    EXPECT_NE(base_source, modified);

    modified.active_user = "";
    EXPECT_NE(base_source, modified);
  }
}

TEST_F(DataProtectionClipboardUtilsTypesTest, FullCopySource_DefaultValues) {
  FullCopySource source;
  EXPECT_FALSE(source.data_transfer_endpoint.has_value());
  EXPECT_EQ(source.browser_context.get(), nullptr);
  EXPECT_FALSE(source.gemini_in_chrome);
  EXPECT_TRUE(source.title.empty());
  EXPECT_TRUE(source.page_content_type.empty());
  EXPECT_TRUE(source.active_user.empty());
  EXPECT_TRUE(source.referrer_chain_data.empty());
  EXPECT_EQ(source.url(), GURL());
  EXPECT_EQ(source, FullCopySource());
}

TEST_F(DataProtectionClipboardUtilsTypesTest,
       FullCopySource_CopyAndAssignment) {
  FullCopySource src;
  src.data_transfer_endpoint =
      ui::DataTransferEndpoint(GURL("https://example.com"));
  src.browser_context = profile1_->GetWeakPtr();
  src.gemini_in_chrome = true;
  src.title = "Page Title";
  src.page_content_type = "text/html";
  src.active_user = "user@example.com";
  auto* entry = src.referrer_chain_data.Add();
  entry->set_url("https://referrer.example.com");
  entry->set_type(safe_browsing::ReferrerChainEntry::EVENT_URL);

  // Copy construction.
  FullCopySource copy_constructed(src);
  EXPECT_EQ(src, copy_constructed);
  EXPECT_EQ(copy_constructed.referrer_chain_data.size(), 1);
  EXPECT_EQ(copy_constructed.referrer_chain_data[0].url(),
            "https://referrer.example.com");

  // Copy assignment.
  FullCopySource copy_assigned;
  copy_assigned = src;
  EXPECT_EQ(src, copy_assigned);

  // Move construction.
  FullCopySource to_move(src);
  FullCopySource move_constructed(std::move(to_move));
  EXPECT_EQ(src, move_constructed);

  // Move assignment.
  FullCopySource to_move_assign(src);
  FullCopySource move_assigned;
  move_assigned = std::move(to_move_assign);
  EXPECT_EQ(src, move_assigned);
}

TEST_F(DataProtectionClipboardUtilsTypesTest,
       FullCopySource_EqualityAndInequality) {
  FullCopySource base_source;
  base_source.data_transfer_endpoint =
      ui::DataTransferEndpoint(GURL("https://example.com"));
  base_source.browser_context = profile1_->GetWeakPtr();
  base_source.gemini_in_chrome = true;
  base_source.title = "Page Title";
  base_source.page_content_type = "text/html";
  base_source.active_user = "user@example.com";
  auto* entry = base_source.referrer_chain_data.Add();
  entry->set_url("https://referrer.example.com");
  entry->set_type(safe_browsing::ReferrerChainEntry::EVENT_URL);

  // Reflexive equality.
  EXPECT_EQ(base_source, base_source);

  // Base fields differences lead to inequality.
  {
    FullCopySource modified = base_source;
    modified.data_transfer_endpoint = std::nullopt;
    EXPECT_NE(base_source, modified);
  }
  {
    FullCopySource modified = base_source;
    modified.browser_context = profile2_->GetWeakPtr();
    EXPECT_NE(base_source, modified);
  }
  {
    FullCopySource modified = base_source;
    modified.gemini_in_chrome = false;
    EXPECT_NE(base_source, modified);
  }
  {
    FullCopySource modified = base_source;
    modified.title = "Different Title";
    EXPECT_NE(base_source, modified);
  }
  {
    FullCopySource modified = base_source;
    modified.page_content_type = "application/pdf";
    EXPECT_NE(base_source, modified);
  }
  {
    FullCopySource modified = base_source;
    modified.active_user = "other_user@example.com";
    EXPECT_NE(base_source, modified);
  }

  // referrer_chain_data differences.
  // 1. Different size (1 vs 0).
  {
    FullCopySource modified = base_source;
    modified.referrer_chain_data.Clear();
    EXPECT_NE(base_source, modified);
  }

  // 2. Different size (1 vs 2).
  {
    FullCopySource modified = base_source;
    auto* entry2 = modified.referrer_chain_data.Add();
    entry2->set_url("https://second.example.com");
    entry2->set_type(safe_browsing::ReferrerChainEntry::CLIENT_REDIRECT);
    EXPECT_NE(base_source, modified);
  }

  // 3. Same size, different URL.
  {
    FullCopySource modified = base_source;
    modified.referrer_chain_data.Mutable(0)->set_url(
        "https://other-referrer.com");
    EXPECT_NE(base_source, modified);
  }

  // 4. Same size, different entry type.
  {
    FullCopySource modified = base_source;
    modified.referrer_chain_data.Mutable(0)->set_type(
        safe_browsing::ReferrerChainEntry::CLIENT_REDIRECT);
    EXPECT_NE(base_source, modified);
  }

  // 5. Multiple identical entries equal.
  {
    auto* entry2 = base_source.referrer_chain_data.Add();
    entry2->set_url("https://second.example.com");
    entry2->set_type(safe_browsing::ReferrerChainEntry::CLIENT_REDIRECT);

    FullCopySource identical = base_source;
    EXPECT_EQ(base_source, identical);

    // 6. Same elements in different order.
    FullCopySource swapped_order = base_source;
    swapped_order.referrer_chain_data.Clear();
    swapped_order.referrer_chain_data.Add()->CopyFrom(
        base_source.referrer_chain_data[1]);
    swapped_order.referrer_chain_data.Add()->CopyFrom(
        base_source.referrer_chain_data[0]);
    EXPECT_NE(base_source, swapped_order);
  }
}

TEST_F(DataProtectionClipboardUtilsTypesTest, CacheBasicPasteSource_Endpoints) {
  // 1. Endpoint without DataTransferEndpoint / BrowserContext / WebContents.
  {
    content::ClipboardEndpoint empty_endpoint(std::nullopt);
    BasicPasteSource cached = CacheBasicPasteSource(empty_endpoint);
    EXPECT_FALSE(cached.data_transfer_endpoint.has_value());
    EXPECT_EQ(cached.browser_context.get(), nullptr);
    EXPECT_FALSE(cached.gemini_in_chrome);
    EXPECT_TRUE(cached.title.empty());
    EXPECT_TRUE(cached.page_content_type.empty());
    EXPECT_EQ(cached.url(), GURL());
  }

  // 2. Endpoint with only DataTransferEndpoint.
  {
    content::ClipboardEndpoint dte_endpoint(
        ui::DataTransferEndpoint(GURL("https://source.example.com")));
    BasicPasteSource cached = CacheBasicPasteSource(dte_endpoint);
    EXPECT_TRUE(cached.data_transfer_endpoint.has_value());
    EXPECT_EQ(cached.url(), GURL("https://source.example.com"));
    EXPECT_EQ(cached.browser_context.get(), nullptr);
    EXPECT_FALSE(cached.gemini_in_chrome);
    EXPECT_TRUE(cached.title.empty());
    EXPECT_TRUE(cached.page_content_type.empty());
  }

  // 3. Endpoint with DataTransferEndpoint and BrowserContext.
  {
    content::ClipboardEndpoint bc_endpoint(
        ui::DataTransferEndpoint(GURL("https://source.example.com")),
        base::BindLambdaForTesting(
            [this]() -> content::BrowserContext* { return profile1_; }));
    BasicPasteSource cached = CacheBasicPasteSource(bc_endpoint);
    EXPECT_TRUE(cached.data_transfer_endpoint.has_value());
    EXPECT_EQ(cached.url(), GURL("https://source.example.com"));
    EXPECT_EQ(cached.browser_context.get(), profile1_);
    EXPECT_FALSE(cached.gemini_in_chrome);
    EXPECT_TRUE(cached.title.empty());
    EXPECT_TRUE(cached.page_content_type.empty());
  }

  // 4. Endpoint with WebContents.
  {
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("https://source.example.com/doc"));
    content::WebContentsTester::For(web_contents())
        ->SetTitle(u"Document Title");

    content::ClipboardEndpoint wc_endpoint(
        ui::DataTransferEndpoint(GURL("https://source.example.com/doc")),
        base::BindLambdaForTesting(
            [this]() -> content::BrowserContext* { return profile1_; }),
        *web_contents()->GetPrimaryMainFrame());
    BasicPasteSource cached = CacheBasicPasteSource(wc_endpoint);
    EXPECT_EQ(cached.url(), GURL("https://source.example.com/doc"));
    EXPECT_EQ(cached.browser_context.get(), profile1_);
    EXPECT_EQ(cached.title, "Document Title");
    EXPECT_EQ(cached.page_content_type, web_contents()->GetContentsMimeType());
    EXPECT_FALSE(cached.gemini_in_chrome);

    // 5. Gemini in Chrome (GlicWebUI).
    glic::CreateGlicWebUiData(web_contents());
    BasicPasteSource cached_glic = CacheBasicPasteSource(wc_endpoint);
    EXPECT_TRUE(cached_glic.gemini_in_chrome);
  }
}

TEST_F(DataProtectionClipboardUtilsTypesTest, CacheFullPasteSource_Endpoints) {
  // Empty endpoint.
  {
    content::ClipboardEndpoint empty_endpoint(std::nullopt);
    FullPasteSource cached = CacheFullPasteSource(empty_endpoint);
    EXPECT_EQ(static_cast<const BasicPasteSource&>(cached),
              CacheBasicPasteSource(empty_endpoint));
  }

  // WebContents endpoint.
  {
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("https://source.example.com/doc"));
    content::ClipboardEndpoint wc_endpoint(
        ui::DataTransferEndpoint(GURL("https://source.example.com/doc")),
        base::BindLambdaForTesting(
            [this]() -> content::BrowserContext* { return profile1_; }),
        *web_contents()->GetPrimaryMainFrame());
    FullPasteSource cached = CacheFullPasteSource(wc_endpoint);
    EXPECT_EQ(static_cast<const BasicPasteSource&>(cached),
              CacheBasicPasteSource(wc_endpoint));
  }
}

TEST_F(DataProtectionClipboardUtilsTypesTest, CacheFullCopySource_Endpoints) {
  // Empty endpoint.
  {
    content::ClipboardEndpoint empty_endpoint(std::nullopt);
    FullCopySource cached = CacheFullCopySource(empty_endpoint);
    EXPECT_EQ(static_cast<const FullPasteSource&>(cached),
              CacheFullPasteSource(empty_endpoint));
    EXPECT_TRUE(cached.referrer_chain_data.empty());
  }

  // WebContents endpoint.
  {
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("https://source.example.com/doc"));
    content::ClipboardEndpoint wc_endpoint(
        ui::DataTransferEndpoint(GURL("https://source.example.com/doc")),
        base::BindLambdaForTesting(
            [this]() -> content::BrowserContext* { return profile1_; }),
        *web_contents()->GetPrimaryMainFrame());
    FullCopySource cached = CacheFullCopySource(wc_endpoint);
    EXPECT_EQ(static_cast<const FullPasteSource&>(cached),
              CacheFullPasteSource(wc_endpoint));
  }
}

}  // namespace

}  // namespace enterprise_data_protection
