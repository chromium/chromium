// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/ai_mode_context_library_converter.h"

#include "components/contextual_search/contextual_search_types.h"
#include "components/sessions/core/session_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "url/gurl.h"

namespace contextual_tasks {

TEST(AiModeContextLibraryConverterTest,
     ConvertWebpageWithMatchingLocalContext) {
  lens::UpdateThreadContextLibrary message;
  auto* context = message.add_contexts();
  context->set_context_id(123);
  auto* webpage = context->mutable_webpage();
  webpage->set_url("https://example.com/page1");
  webpage->set_title("Example Page 1");

  std::vector<contextual_search::FileInfo> local_contexts;
  contextual_search::FileInfo file_info;
  file_info.request_id.set_context_id(123);
  file_info.tab_url = GURL("https://example.com/page1");
  file_info.tab_title = "Local Title 1";
  file_info.tab_session_id = SessionID::FromSerializedValue(10);
  local_contexts.push_back(file_info);

  std::vector<UrlResource> url_resources =
      ConvertAiModeContextToUrlResources(message, local_contexts);

  ASSERT_EQ(url_resources.size(), 1u);
  EXPECT_EQ(url_resources[0].url, GURL("https://example.com/page1"));
  EXPECT_EQ(url_resources[0].title, "Example Page 1");
  EXPECT_TRUE(url_resources[0].tab_id.has_value());
  EXPECT_EQ(url_resources[0].tab_id->id(), 10);
  EXPECT_EQ(url_resources[0].context_id, 123u);
}

TEST(AiModeContextLibraryConverterTest,
     ConvertWebpageWithoutMatchingLocalContext) {
  lens::UpdateThreadContextLibrary message;
  auto* context = message.add_contexts();
  context->set_context_id(456);
  auto* webpage = context->mutable_webpage();
  webpage->set_url("https://example.com/page2");
  webpage->set_title("Example Page 2");

  std::vector<contextual_search::FileInfo> local_contexts;
  // Empty local contexts or mismatching IDs
  contextual_search::FileInfo file_info;
  file_info.request_id.set_context_id(999);
  local_contexts.push_back(file_info);

  std::vector<UrlResource> url_resources =
      ConvertAiModeContextToUrlResources(message, local_contexts);

  ASSERT_EQ(url_resources.size(), 1u);
  EXPECT_EQ(url_resources[0].url, GURL("https://example.com/page2"));
  EXPECT_EQ(url_resources[0].title, "Example Page 2");
  EXPECT_FALSE(url_resources[0].tab_id.has_value());
  EXPECT_EQ(url_resources[0].context_id, 456u);
}

TEST(AiModeContextLibraryConverterTest, ConvertPdfContext) {
  lens::UpdateThreadContextLibrary message;
  auto* context = message.add_contexts();
  context->set_context_id(789);
  auto* pdf = context->mutable_pdf();
  pdf->set_url("https://example.com/doc.pdf");
  pdf->set_title("PDF Document");

  std::vector<contextual_search::FileInfo> local_contexts;

  std::vector<UrlResource> url_resources =
      ConvertAiModeContextToUrlResources(message, local_contexts);

  ASSERT_EQ(url_resources.size(), 1u);
  EXPECT_EQ(url_resources[0].url, GURL("https://example.com/doc.pdf"));
  EXPECT_EQ(url_resources[0].title, "PDF Document");
  EXPECT_FALSE(url_resources[0].tab_id.has_value());
  EXPECT_EQ(url_resources[0].context_id, 789u);
}

TEST(AiModeContextLibraryConverterTest, ConvertImageContext) {
  lens::UpdateThreadContextLibrary message;
  auto* context = message.add_contexts();
  context->set_context_id(101);
  auto* image = context->mutable_image();
  image->set_url("https://example.com/img.png");
  image->set_title("Image Document");

  std::vector<contextual_search::FileInfo> local_contexts;

  std::vector<UrlResource> url_resources =
      ConvertAiModeContextToUrlResources(message, local_contexts);

  ASSERT_EQ(url_resources.size(), 1u);
  EXPECT_EQ(url_resources[0].url, GURL("https://example.com/img.png"));
  EXPECT_EQ(url_resources[0].title, "Image Document");
  EXPECT_FALSE(url_resources[0].tab_id.has_value());
  EXPECT_EQ(url_resources[0].context_id, 101u);
}

TEST(AiModeContextLibraryConverterTest, ConvertUnknownContext) {
  lens::UpdateThreadContextLibrary message;
  auto* context = message.add_contexts();
  context->set_context_id(202);
  // No webpage, pdf, or image set.

  std::vector<contextual_search::FileInfo> local_contexts;

  std::vector<UrlResource> url_resources =
      ConvertAiModeContextToUrlResources(message, local_contexts);

  ASSERT_EQ(url_resources.size(), 1u);
  EXPECT_TRUE(url_resources[0].url.is_empty());
  EXPECT_TRUE(url_resources[0].context_id.has_value());
  EXPECT_EQ(url_resources[0].context_id, 202u);
}

TEST(AiModeContextLibraryConverterTest, ConvertMixedContexts) {
  lens::UpdateThreadContextLibrary message;

  // 1. Webpage
  auto* c1 = message.add_contexts();
  c1->set_context_id(1);
  c1->mutable_webpage()->set_url("http://web.com");
  c1->mutable_webpage()->set_title("Webpage");

  // 2. PDF
  auto* c2 = message.add_contexts();
  c2->set_context_id(2);
  c2->mutable_pdf()->set_url("http://pdf.com");
  c2->mutable_pdf()->set_title("PDF");

  // 3. Image
  auto* c3 = message.add_contexts();
  c3->set_context_id(3);
  c3->mutable_image()->set_url("http://img.com");
  c3->mutable_image()->set_title("Image");

  // Local contexts for Webpage
  std::vector<contextual_search::FileInfo> local_contexts;
  contextual_search::FileInfo fi;
  fi.request_id.set_context_id(1);
  fi.tab_url = GURL("http://web.com");
  fi.tab_title = "Web Title";
  fi.tab_session_id = SessionID::FromSerializedValue(50);
  local_contexts.push_back(fi);

  std::vector<UrlResource> result =
      ConvertAiModeContextToUrlResources(message, local_contexts);

  ASSERT_EQ(result.size(), 3u);

  // Check Webpage
  EXPECT_EQ(result[0].context_id, 1u);
  EXPECT_EQ(result[0].url, GURL("http://web.com"));
  EXPECT_EQ(result[0].title, "Webpage");
  EXPECT_EQ(result[0].tab_id->id(), 50);

  // Check PDF
  EXPECT_EQ(result[1].context_id, 2u);
  EXPECT_EQ(result[1].url, GURL("http://pdf.com"));
  EXPECT_EQ(result[1].title, "PDF");

  // Check Image
  EXPECT_EQ(result[2].context_id, 3u);
  EXPECT_EQ(result[2].url, GURL("http://img.com"));
  EXPECT_EQ(result[2].title, "Image");
}

}  // namespace contextual_tasks
