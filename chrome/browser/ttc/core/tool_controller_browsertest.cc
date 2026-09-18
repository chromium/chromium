// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/tool_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/tool_types.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

class ToolControllerBrowserTest : public TtcCoreBrowserTestBase {
 public:
  ToolControllerBrowserTest() = default;
  ~ToolControllerBrowserTest() override = default;

  // TtcCoreBrowserTestBase:
  void SetUpOnMainThread() override {
    TtcCoreBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, OpenUrlCurrentTab) {
  // Verify ActorKeyedService is available.
  auto* actor_service = actor::ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ToolRequest tool_request;
  tool_request.name = "open_url";
  tool_request.arguments.Set("url", url.spec());
  tool_request.arguments.Set("new_tab", false);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());

  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, UnsupportedTool) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name = "unsupported_tool";

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kToolUnknown);
  ASSERT_TRUE(response.error().message.has_value());
  EXPECT_EQ(*response.error().message, "Unsupported tool");
}

}  // namespace

}  // namespace ttc
