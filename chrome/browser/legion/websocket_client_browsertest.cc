// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/legion/client.h"
#include "components/legion/features.h"
#include "components/legion/proto/legion.pb.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace legion {

namespace {

const base::FeatureParam<std::string> kTestFeatureName{
    &kLegion, "test-feature-name", "FEATURE_NAME_UNSPECIFIED"};
const base::FeatureParam<std::string> kTestQueryText{
    &kLegion, "test-query-text", "Hello Legion!"};

// This class allows manual testing of the Legion Service.
class LegionWebSocketClientBrowserTest : public InProcessBrowserTest {
 public:
  LegionWebSocketClientBrowserTest() {
    SetAllowNetworkAccessToHostResolutions();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LegionWebSocketClientBrowserTest, MANUAL_Client) {
  const std::string feature_name_str = kTestFeatureName.Get();
  CHECK(!feature_name_str.empty())
      << "Missing test-feature-name param for Legion feature. Please provide a "
         "feature name."
      << "--enable-features=Legion:test-feature-name/FEATURE_NAME_UNSPECIFIED";

  legion::proto::FeatureName feature_name;
  CHECK(legion::proto::FeatureName_Parse(feature_name_str, &feature_name))
      << "Invalid feature name: " << feature_name_str;

  const std::string text = kTestQueryText.Get();
  CHECK(!text.empty()) << "Missing test-query-text param for Legion feature. "
                          "Please provide a query text."
                       << "--enable-features=Legion:test-query-text/'Hello "
                          "Legion!'";
  auto* network_context =
      browser()->profile()->GetDefaultStoragePartition()->GetNetworkContext();
  auto client = Client::Create(network_context);

  base::test::TestFuture<base::expected<std::string, ErrorCode>> future;
  client->SendTextRequest(feature_name, text, future.GetCallback());

  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value().empty());

  // Log the response for manual verification.
  LOG(INFO) << "Response from Legion: " << result.value();
}

}  // namespace

}  // namespace legion
