// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/shared_types.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {
namespace {

using ::content::EvalJs;
using ::content::GetDOMNodeId;
using ::optimization_guide::DocumentIdentifierUserData;

std::optional<DomNode> GetDomNodeOnPage(content::RenderFrameHost& rfh,
                                        std::string_view query_selector) {
  std::optional<int> node_id = GetDOMNodeId(rfh, query_selector);
  if (!node_id) {
    return std::nullopt;
  }
  std::optional<std::string> document_identifier =
      DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken());
  if (!document_identifier) {
    return std::nullopt;
  }
  return DomNode{.node_id = *node_id,
                 .document_identifier = std::move(*document_identifier)};
}

class ChangePasswordToolBrowserTest : public ActorToolsTest {
 public:
  ChangePasswordToolBrowserTest() = default;
  ~ChangePasswordToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_AllFields_Succeeds) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  GetPageApc();

  std::optional<DomNode> current_password_node =
      GetDomNodeOnPage(*main_frame(), "#input");
  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#input2");
  std::optional<DomNode> confirm_password_node =
      GetDomNodeOnPage(*main_frame(), "#keyHandlingInput");
  ASSERT_TRUE(current_password_node);
  ASSERT_TRUE(new_password_node);
  ASSERT_TRUE(confirm_password_node);

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          PageTarget(*current_password_node), PageTarget(*new_password_node),
          PageTarget(*confirm_password_node),
          /*confirm_button=*/std::nullopt);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ("dummy_current_password",
            EvalJs(main_frame(), "document.getElementById('input').value"));
  EXPECT_EQ("dummy_new_password",
            EvalJs(main_frame(), "document.getElementById('input2').value"));
  EXPECT_EQ("dummy_new_password",
            EvalJs(main_frame(),
                   "document.getElementById('keyHandlingInput').value"));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_PartialFields_Succeeds) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  GetPageApc();

  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#input2");
  ASSERT_TRUE(new_password_node);

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          /*current_password_field=*/std::nullopt,
          PageTarget(*new_password_node),
          /*confirm_new_password_field=*/std::nullopt,
          /*confirm_button=*/std::nullopt);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ("", EvalJs(main_frame(), "document.getElementById('input').value"));
  EXPECT_EQ("dummy_new_password",
            EvalJs(main_frame(), "document.getElementById('input2').value"));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_FillFails_ReturnsError) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  GetPageApc();

  std::optional<DomNode> select_node =
      GetDomNodeOnPage(*main_frame(), "#plainSelect");
  ASSERT_TRUE(select_node);

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          /*current_password_field=*/std::nullopt, PageTarget(*select_node),
          /*confirm_new_password_field=*/std::nullopt,
          /*confirm_button=*/std::nullopt);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFormFillingUnknownAutofillError);
}

}  // namespace
}  // namespace actor
