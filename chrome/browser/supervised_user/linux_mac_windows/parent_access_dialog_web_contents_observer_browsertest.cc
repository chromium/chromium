// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_dialog_web_contents_observer.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPacpHost[] = "families.google.com";

struct TestParam {
  // Whether the end url `kFamilyManagementUrl` in the sequence of PACP server
  // redirections is reached.
  bool redirect_to_target_url;
  // Query paramerer containing the PACP parent approval result, if such a
  // result should be returned.
  std::optional<std::string> result_query_param;
};

// TODO(crbug.com/383997522): Extend the test case with the actual query result
// verification, when this logic added to the observer.
class SupervisedUserParentAccessObserverTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  void MockCompletionCallback(supervised_user::LocalApprovalResult result) {
    is_callback_executed_ = true;
  }

 protected:
  bool IsCompletionCallbackExecuted() {
    // TODO(crbug.com/383997522): Once result extraction is supported,
    // check the expected result.
    return is_callback_executed_;
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  supervised_user::SupervisionMixin& supervision_mixin() {
    return supervision_mixin_;
  }

 private:
  void SetUp() override {
    // Mock the responses to navigations via the test server.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SupervisedUserParentAccessObserverTest::HandleRedirection,
        base::Unretained(this)));
    MixinBasedInProcessBrowserTest::SetUp();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRedirection(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (GetParam().redirect_to_target_url) {
      if (request.GetURL().has_query()) {
        CHECK(request.GetURL().query().starts_with("result"));
      }
      // Mock a url-redirection to the PACP target url.
      // Mimics the last url in a seriers of PACP re-directions.
      response->set_code(net::HTTP_MOVED_PERMANENTLY);
      response->AddCustomHeader("Location",
                                supervised_user::kFamilyManagementUrl);
      return response;
    }
    response->set_code(net::HTTP_OK);
    return response;
  }

  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode =
           supervised_user::SupervisionMixin::SignInMode::kSupervised,
       .embedded_test_server_options = {.resolver_rules_map_host_list =
                                            kPacpHost}}};

  bool is_callback_executed_ = false;
};

IN_PROC_BROWSER_TEST_P(SupervisedUserParentAccessObserverTest,
                       CompletionCallbackExecution) {
  CHECK(contents());

  base::OnceCallback<void(supervised_user::LocalApprovalResult result)>
      completion_callback = base::BindOnce(
          &SupervisedUserParentAccessObserverTest::MockCompletionCallback,
          base::Unretained(this));
  auto dialog_web_contents_observer =
      std::make_unique<ParentAccessDialogWebContentsObserver>(
          contents(),
          /*url_approval_result_callback=*/std::move(completion_callback));

  CHECK(dialog_web_contents_observer);
  GURL::Replacements result_query_param;
  if (GetParam().result_query_param.has_value()) {
    result_query_param.SetQueryStr(GetParam().result_query_param.value());
  }

  // Mimic navigating to a url that may contain the query result.
  supervision_mixin()
      .api_mock_setup_mixin()
      .api_mock()
      .AllowSubsequentClassifyUrl();
  GURL pacp_origin_url_with_optional_result =
      embedded_test_server()
          ->GetURL(kPacpHost, "/")
          .ReplaceComponents(result_query_param);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), pacp_origin_url_with_optional_result));

  // The callback is executed at the end of the navigation if navigation
  // reaches the target url and a query result has been parsed.
  bool should_execute_completion_callback =
      GetParam().result_query_param.has_value() &&
      GetParam().redirect_to_target_url;
  EXPECT_EQ(should_execute_completion_callback, IsCompletionCallbackExecuted());
}

std::string ParamToTestName(TestParam param) {
  std::string name = param.result_query_param.has_value()
                         ? "WithQueryResult"
                         : "WithoutQueryResult";
  name = name + (param.redirect_to_target_url ? "WhenEndPacpUrlReached"
                                              : "WhenEndPacpUrlNotReached");
  return name;
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserParentAccessObserverTest,
    testing::Values(TestParam({.redirect_to_target_url = true,
                               .result_query_param = "result=MgA"}),
                    TestParam({.redirect_to_target_url = false,
                               .result_query_param = "result=MgA"}),
                    TestParam({.redirect_to_target_url = true,
                               .result_query_param = std::nullopt})),
    [](const auto& info) { return ParamToTestName(info.param); });

}  // namespace
