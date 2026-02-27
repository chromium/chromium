// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/private_ai/private_ai_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/private_ai/client.h"
#include "components/private_ai/features.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

const base::FeatureParam<std::string> kTestFeatureName{
    &kPrivateAi, "test-feature-name", "FEATURE_NAME_UNSPECIFIED"};
const base::FeatureParam<std::string> kTestQueryText{
    &kPrivateAi, "test-query-text", "Hello PrivateAI!"};

// This class allows manual testing of the PrivateAI Service.
class PrivateAiWebSocketClientBrowserTest : public InProcessBrowserTest {
 public:
  PrivateAiWebSocketClientBrowserTest() {
    SetAllowNetworkAccessToHostResolutions();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateAiWebSocketClientBrowserTest, MANUAL_Client) {
  const std::string feature_name_str = kTestFeatureName.Get();
  CHECK(!feature_name_str.empty()) << "Missing test-feature-name param for "
                                      "PrivateAI feature. Please provide a "
                                      "feature name."
                                   << "--enable-features=PrivateAi:test-"
                                      "feature-name/FEATURE_NAME_UNSPECIFIED";

  proto::FeatureName feature_name;
  CHECK(proto::FeatureName_Parse(feature_name_str, &feature_name))
      << "Invalid feature name: " << feature_name_str;

  const std::string text = kTestQueryText.Get();
  CHECK(!text.empty())
      << "Missing test-query-text param for PrivateAI feature. "
         "Please provide a query text."
      << "--enable-features=PrivateAi:test-query-text/'Hello "
         "PrivateAI!'";
  auto* private_ai_service =
      PrivateAiServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(private_ai_service);
  auto* client = private_ai_service->GetClient();
  ASSERT_TRUE(client);

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client->SendTextRequest(feature_name, text, future.GetCallback(),
                          /*options=*/{});

  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().empty());

  // Log the response for manual verification.
  LOG(INFO) << "Response from PrivateAI: " << result.value();
}

}  // namespace

}  // namespace private_ai
