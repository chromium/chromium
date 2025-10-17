// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/websocket_client.h"

#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/legion/features.h"
#include "components/legion/transport.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "third_party/oak/chromium/proto/session/session.test.h"
#include "third_party/oak/chromium/proto/session/session.to_value.h"

namespace legion {

// This class allows manual testing of the Legion Service.
class LegionWebSocketClientBrowserTest : public InProcessBrowserTest {
 public:
  LegionWebSocketClientBrowserTest() {
    SetAllowNetworkAccessToHostResolutions();
  }

  GURL url() {
    return GURL(base::StrCat({"wss://" + legion::kLegionUrl.Get() + "?key=",
                              legion::kLegionApiKey.Get()}));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LegionWebSocketClientBrowserTest,
                       MANUAL_WriteTestRequest) {
  base::test::TestFuture<base::expected<oak::session::v1::SessionResponse,
                                        Transport::TransportError>>
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
          browser()));

  Transport* transport = client.get();

  oak::session::v1::SessionRequest request;
  request.mutable_attest_request();
  LOG(ERROR) << "Request: " << oak::session::v1::Serialize(request);
  transport->Send(std::move(request), future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());

  const auto& response = result.value();
  LOG(ERROR) << "Response: " << oak::session::v1::Serialize(response);
  EXPECT_FALSE(true);  // Fail test to see log output.
}

}  // namespace legion
