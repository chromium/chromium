// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_inference/websocket_client.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/private_inference/features.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using private_inference::WebSocketClient;

// This class allows manual testing of the Private Inference Service.
class PrivateInferenceWebSocketClientBrowserTest : public InProcessBrowserTest {
 public:
  PrivateInferenceWebSocketClientBrowserTest() {
    SetAllowNetworkAccessToHostResolutions();
  }

  GURL url() {
    return GURL(base::StrCat(
        {"wss://" + private_inference::kPrivateInferenceUrl.Get() + "?key=",
         private_inference::kPrivateInferenceApiKey.Get()}));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateInferenceWebSocketClientBrowserTest,
                       MANUAL_WriteTestRequest) {
  base::test::TestFuture<WebSocketClient::SocketStatus, std::vector<uint8_t>>
      future;
  LOG(ERROR) << "Connecting: " << url();
  auto client = std::make_unique<WebSocketClient>(
      url(),
      base::BindRepeating(
          [](Browser* browser) -> network::mojom::NetworkContext* {
            return browser->profile()
                ->GetDefaultStoragePartition()
                ->GetNetworkContext();
          },
          browser()),
      future.GetRepeatingCallback());

  std::string message =
      R"({"attestRequest":{"assertions":{},"endorsedEvidence":{}}})";
  std::vector<uint8_t> data(message.begin(), message.end());
  client->Write(data);

  auto [status, response] = future.Get();
  EXPECT_EQ(status, WebSocketClient::SocketStatus::kOk);
  EXPECT_FALSE(response.empty());
  LOG(ERROR) << "Response: " << std::string(response.begin(), response.end());
  EXPECT_FALSE(true);  // Fail test to see log output.
}
