// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/shared_types.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "url/gurl.h"

namespace actor {
namespace {

using ::content::EvalJs;

class ChangePasswordToolBrowserTest : public ActorToolsTest {
 public:
  ChangePasswordToolBrowserTest() = default;
  ~ChangePasswordToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  [[nodiscard]] bool NavigateAndCaptureApc() {
    const GURL url = embedded_https_test_server().GetURL(
        "example.com", "/actor/change_password.html");
    if (!content::NavigateToURL(web_contents(), url)) {
      return false;
    }
    GetPageApc();
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_AllFields_Succeeds) {
  ASSERT_TRUE(NavigateAndCaptureApc());

  std::optional<DomNode> current_password_node =
      GetDomNodeOnPage(*main_frame(), "#current_password");
  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
  std::optional<DomNode> confirm_password_node =
      GetDomNodeOnPage(*main_frame(), "#confirm_password");
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
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(
      result.Get()[0].result->message,
      absl::StrFormat(
          "Filled old password into field with id %d and new generated "
          "password into field with id %d and confirmation password into field "
          "with id %d.",
          current_password_node->node_id, new_password_node->node_id,
          confirm_password_node->node_id));

  EXPECT_EQ("dummy_current_password",
            EvalJs(main_frame(),
                   "document.getElementById('current_password').value"));
  EXPECT_EQ(
      "dummy_new_password",
      EvalJs(main_frame(), "document.getElementById('new_password').value"));
  EXPECT_EQ("dummy_new_password",
            EvalJs(main_frame(),
                   "document.getElementById('confirm_password').value"));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_PartialFields_Succeeds) {
  ASSERT_TRUE(NavigateAndCaptureApc());

  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
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
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(
      result.Get()[0].result->message,
      absl::StrFormat("Filled new generated password into field with id %d.",
                      new_password_node->node_id));

  EXPECT_EQ("", EvalJs(main_frame(),
                       "document.getElementById('current_password').value"));
  EXPECT_EQ(
      "dummy_new_password",
      EvalJs(main_frame(), "document.getElementById('new_password').value"));
  EXPECT_EQ("", EvalJs(main_frame(),
                       "document.getElementById('confirm_password').value"));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_FillFails_ReturnsError) {
  ASSERT_TRUE(NavigateAndCaptureApc());

  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
  ASSERT_TRUE(new_password_node);

  ASSERT_TRUE(content::ExecJs(
      main_frame(), "document.getElementById('new_password').remove();"));

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          /*current_password_field=*/std::nullopt,
          PageTarget(*new_password_node),
          /*confirm_new_password_field=*/std::nullopt,
          /*confirm_button=*/std::nullopt);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].result->message, "Found no eligible form to fill.");
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_SomeFieldsFailToFill_ReturnsError) {
  ASSERT_TRUE(NavigateAndCaptureApc());

  std::optional<DomNode> current_password_node =
      GetDomNodeOnPage(*main_frame(), "#current_password");
  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
  std::optional<DomNode> confirm_button_node =
      GetDomNodeOnPage(*main_frame(), "#submit");
  ASSERT_TRUE(current_password_node);
  ASSERT_TRUE(new_password_node);
  ASSERT_TRUE(confirm_button_node);

  ASSERT_TRUE(content::ExecJs(
      main_frame(), "document.getElementById('new_password').remove();"));

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          PageTarget(*current_password_node), PageTarget(*new_password_node),
          /*confirm_new_password_field=*/std::nullopt,
          PageTarget(*confirm_button_node));

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].result->message,
            absl::StrFormat("Filled old password into field with id %d. Failed "
                            "to fill new generated password into field with id "
                            "%d.",
                            current_password_node->node_id,
                            new_password_node->node_id));
  EXPECT_THAT(EvalJs(main_frame(), "getStableEventLog()").ExtractString(),
              testing::Not(testing::HasSubstr("click")));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_MultipleFieldsFailToFill_ReturnsError) {
  ASSERT_TRUE(NavigateAndCaptureApc());

  std::optional<DomNode> current_password_node =
      GetDomNodeOnPage(*main_frame(), "#current_password");
  std::optional<DomNode> cleared_input_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
  std::optional<DomNode> confirm_password_node =
      GetDomNodeOnPage(*main_frame(), "#confirm_password");
  ASSERT_TRUE(current_password_node);
  ASSERT_TRUE(cleared_input_node);
  ASSERT_TRUE(confirm_password_node);

  ASSERT_TRUE(content::ExecJs(
      main_frame(),
      "document.getElementById('new_password').addEventListener("
      "    'keyup', e => { e.target.value = ''; });"
      "document.getElementById('confirm_password').remove();"));

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          PageTarget(*current_password_node), PageTarget(*cleared_input_node),
          PageTarget(*confirm_password_node),
          /*confirm_button=*/std::nullopt);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(
      result.Get()[0].result->message,
      absl::StrFormat(
          "Filled old password into field with id %d. Failed to fill new "
          "generated password into field with id %d and confirmation password "
          "into field with id %d.",
          current_password_node->node_id, cleared_input_node->node_id,
          confirm_password_node->node_id));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_FieldNotAutofilled_ReturnsError) {
  ASSERT_TRUE(NavigateAndCaptureApc());
  ASSERT_TRUE(content::ExecJs(
      main_frame(),
      "document.getElementById('new_password').addEventListener("
      "    'keyup', e => { e.target.value = ''; });"));

  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
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
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].result->message,
            absl::StrFormat(
                "Failed to fill new generated password into field with id %d.",
                new_password_node->node_id));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_WithConfirmButton_ClicksButton) {
  ASSERT_TRUE(NavigateAndCaptureApc());

  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
  std::optional<DomNode> confirm_button_node =
      GetDomNodeOnPage(*main_frame(), "#submit");
  ASSERT_TRUE(new_password_node);
  ASSERT_TRUE(confirm_button_node);

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          /*current_password_field=*/std::nullopt,
          PageTarget(*new_password_node),
          /*confirm_new_password_field=*/std::nullopt,
          PageTarget(*confirm_button_node));

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].result->message,
            absl::StrFormat(
                "Filled new generated password into field with id %d. "
                "Triggered click on confirm button with id %d.",
                new_password_node->node_id, confirm_button_node->node_id));

  EXPECT_EQ(
      "dummy_new_password",
      EvalJs(main_frame(), "document.getElementById('new_password').value"));
  EXPECT_EQ("click", EvalJs(main_frame(), "getStableEventLog()"));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_AllFieldsWithConfirmButton_ClicksButton) {
  ASSERT_TRUE(NavigateAndCaptureApc());

  std::optional<DomNode> current_password_node =
      GetDomNodeOnPage(*main_frame(), "#current_password");
  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
  std::optional<DomNode> confirm_password_node =
      GetDomNodeOnPage(*main_frame(), "#confirm_password");
  std::optional<DomNode> confirm_button_node =
      GetDomNodeOnPage(*main_frame(), "#submit");
  ASSERT_TRUE(current_password_node);
  ASSERT_TRUE(new_password_node);
  ASSERT_TRUE(confirm_password_node);
  ASSERT_TRUE(confirm_button_node);

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          PageTarget(*current_password_node), PageTarget(*new_password_node),
          PageTarget(*confirm_password_node), PageTarget(*confirm_button_node));

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(
      result.Get()[0].result->message,
      absl::StrFormat(
          "Filled old password into field with id %d and new generated "
          "password into field with id %d and confirmation password into field "
          "with id %d. Triggered click on confirm button with id %d.",
          current_password_node->node_id, new_password_node->node_id,
          confirm_password_node->node_id, confirm_button_node->node_id));

  EXPECT_EQ("dummy_current_password",
            EvalJs(main_frame(),
                   "document.getElementById('current_password').value"));
  EXPECT_EQ(
      "dummy_new_password",
      EvalJs(main_frame(), "document.getElementById('new_password').value"));
  EXPECT_EQ("dummy_new_password",
            EvalJs(main_frame(),
                   "document.getElementById('confirm_password').value"));
  EXPECT_THAT(EvalJs(main_frame(), "getStableEventLog()").ExtractString(),
              testing::EndsWith("click"));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_WithCoordinateConfirmButton_ClicksButton) {
  ASSERT_TRUE(NavigateAndCaptureApc());

  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
  ASSERT_TRUE(new_password_node);
  gfx::Point confirm_button_point = gfx::ToFlooredPoint(
      content::GetCenterCoordinatesOfElementWithId(web_contents(), "submit"));

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          /*current_password_field=*/std::nullopt,
          PageTarget(*new_password_node),
          /*confirm_new_password_field=*/std::nullopt,
          PageTarget(confirm_button_point));

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].result->message,
            absl::StrFormat(
                "Filled new generated password into field with id %d. "
                "Triggered click on confirm button %s.",
                new_password_node->node_id, confirm_button_point.ToString()));

  EXPECT_EQ(
      "dummy_new_password",
      EvalJs(main_frame(), "document.getElementById('new_password').value"));
  EXPECT_EQ("click", EvalJs(main_frame(), "getStableEventLog()"));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_FillFails_DoesNotClickConfirmButton) {
  ASSERT_TRUE(NavigateAndCaptureApc());

  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
  std::optional<DomNode> confirm_button_node =
      GetDomNodeOnPage(*main_frame(), "#submit");
  ASSERT_TRUE(new_password_node);
  ASSERT_TRUE(confirm_button_node);

  ASSERT_TRUE(content::ExecJs(
      main_frame(), "document.getElementById('new_password').remove();"));

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          /*current_password_field=*/std::nullopt,
          PageTarget(*new_password_node),
          /*confirm_new_password_field=*/std::nullopt,
          PageTarget(*confirm_button_node));

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(result.Get()[0].result->message, "Found no eligible form to fill.");
  EXPECT_EQ("", EvalJs(main_frame(), "getStableEventLog()"));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_WithDisabledConfirmButton_FailsClickAfterFill) {
  ASSERT_TRUE(NavigateAndCaptureApc());
  ASSERT_TRUE(content::ExecJs(
      main_frame(), "document.getElementById('submit').disabled = true;"));

  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
  std::optional<DomNode> confirm_button_node =
      GetDomNodeOnPage(*main_frame(), "#submit");
  ASSERT_TRUE(new_password_node);
  ASSERT_TRUE(confirm_button_node);

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          /*current_password_field=*/std::nullopt,
          PageTarget(*new_password_node),
          /*confirm_new_password_field=*/std::nullopt,
          PageTarget(*confirm_button_node));

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kElementDisabled);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(
      "dummy_new_password",
      EvalJs(main_frame(), "document.getElementById('new_password').value"));
  EXPECT_EQ("", EvalJs(main_frame(), "getStableEventLog()"));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_PasswordFieldsOutsideForm_Succeeds) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/change_password_without_form.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  GetPageApc();

  std::optional<DomNode> current_password_node =
      GetDomNodeOnPage(*main_frame(), "#current_password");
  std::optional<DomNode> new_password_node =
      GetDomNodeOnPage(*main_frame(), "#new_password");
  std::optional<DomNode> confirm_password_node =
      GetDomNodeOnPage(*main_frame(), "#confirm_password");
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
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ("dummy_current_password",
            EvalJs(main_frame(),
                   "document.getElementById('current_password').value"));
  EXPECT_EQ(
      "dummy_new_password",
      EvalJs(main_frame(), "document.getElementById('new_password').value"));
  EXPECT_EQ("dummy_new_password",
            EvalJs(main_frame(),
                   "document.getElementById('confirm_password').value"));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_NoPasswordFormOnPage_ReturnsError) {
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
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(
      result.Get()[0].result->message,
      absl::StrFormat(
          "Target field is not a valid password field: new_password_field "
          "(id %d).",
                      select_node->node_id));
}

IN_PROC_BROWSER_TEST_F(ChangePasswordToolBrowserTest,
                       Invoke_NonPasswordFieldPassedAsTarget_ReturnsError) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/sign_in_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  GetPageApc();

  std::optional<DomNode> password_node =
      GetDomNodeOnPage(*main_frame(), "#password");
  std::optional<DomNode> username_node =
      GetDomNodeOnPage(*main_frame(), "#username");
  ASSERT_TRUE(password_node);
  ASSERT_TRUE(username_node);

  std::unique_ptr<ToolRequest> request =
      std::make_unique<ChangePasswordToolRequest>(
          ActorSurfaceHandle::From(active_tab()->GetHandle()),
          PageTarget(*password_node), PageTarget(*username_node),
          /*confirm_new_password_field=*/std::nullopt,
          /*confirm_button=*/std::nullopt);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  ASSERT_EQ(result.Get().size(), 1u);
  EXPECT_EQ(
      result.Get()[0].result->message,
      absl::StrFormat(
          "Target field is not a valid password field: new_password_field "
          "(id %d).",
                      username_node->node_id));
  EXPECT_EQ("",
            EvalJs(main_frame(), "document.getElementById('password').value"));
  EXPECT_EQ("",
            EvalJs(main_frame(), "document.getElementById('username').value"));
}

}  // namespace
}  // namespace actor
