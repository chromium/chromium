// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_dialog_result_observer.h"

#include <optional>
#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/parent_access_test_utils.h"
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
  // Expected local approval result, when a query parameter with a result is
  // provided in the PACP response url.
  std::optional<supervised_user::LocalApprovalResult> expected_approval_result;
  // Expected error type in the case of an error result.
  std::optional<supervised_user::LocalWebApprovalErrorType> expected_error_type;
  // An string to be appended in the test name.
  std::string test_name_suffix;
};

std::string CreateInvalidEncodingResult() {
  // Non base64 characters.
  return "*INVALID*CHARS";
}

std::string CreateInvalidPacpResponse() {
  // Non PACP response.
  return base::Base64Encode("invalid_response");
}

std::string GetPacpApprovalResultMatchingForgivingDecoding() {
  // Returns a result that can be decoded only in base64 forgiving decoding
  // mode.
  std::string encoded_result = supervised_user::CreatePacpApprovalResult();

  // Make the input size non divisible by 4 in order to fail strict decoding.
  // Remove the padding if there was any.
  bool has_padding = false;
  while (encoded_result.back() == '=') {
    has_padding = true;
    encoded_result.pop_back();
  }
  // If there was no padding to be removed, add an empty space instead.
  if (!has_padding) {
    encoded_result += "\n";
  }
  std::string strict_decoding_attempt;
  CHECK(!base::Base64Decode(encoded_result, &strict_decoding_attempt,
                            base::Base64DecodePolicy::kStrict));
  return encoded_result;
}

class SupervisedUserParentAccessObserverTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  void MockCompletionCallback(
      supervised_user::LocalApprovalResult result,
      std::optional<supervised_user::LocalWebApprovalErrorType> error_type) {
    is_callback_executed_ = true;
    extracted_local_approval_result_ = result;
    extracted_error_type_ = error_type;
  }

 protected:
  bool IsCompletionCallbackExecuted() { return is_callback_executed_; }

  std::optional<supervised_user::LocalApprovalResult>
  extracted_local_approval_result() {
    return extracted_local_approval_result_;
  }

  std::optional<supervised_user::LocalWebApprovalErrorType>
  extracted_error_type() {
    return extracted_error_type_;
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
        CHECK(request.GetURL().GetQuery().starts_with("result"));
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
  std::optional<supervised_user::LocalApprovalResult>
      extracted_local_approval_result_;
  std::optional<supervised_user::LocalWebApprovalErrorType>
      extracted_error_type_;
};

IN_PROC_BROWSER_TEST_P(SupervisedUserParentAccessObserverTest,
                       CompletionCallbackExecution) {
  CHECK(contents());
  base::HistogramTester histogram_tester;

  base::OnceCallback<void(
      supervised_user::LocalApprovalResult result,
      std::optional<supervised_user::LocalWebApprovalErrorType>)>
      completion_callback = base::BindOnce(
          &SupervisedUserParentAccessObserverTest::MockCompletionCallback,
          base::Unretained(this));
  auto dialog_web_contents_observer =
      std::make_unique<ParentAccessDialogResultObserver>(
          /*url_approval_result_callback=*/std::move(completion_callback));
  CHECK(dialog_web_contents_observer);
  dialog_web_contents_observer->StartObserving(contents());

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

  // The callback is executed at the end of the navigation if
  // 1) the navigation reaches the target url and
  // 2) a query result for the approval flow has been parsed or an error was
  // detected.
  bool should_execute_completion_callback =
      GetParam().expected_approval_result.has_value() &&
      GetParam().redirect_to_target_url;
  EXPECT_EQ(should_execute_completion_callback, IsCompletionCallbackExecuted());

  if (should_execute_completion_callback) {
    // If we expect a valid local web approval result from the the callback,
    // check that it is the expected one.
    EXPECT_TRUE(extracted_local_approval_result().has_value());
    EXPECT_EQ(GetParam().expected_approval_result.value(),
              extracted_local_approval_result().value());
  }

  if (GetParam().expected_error_type.has_value()) {
    ASSERT_TRUE(extracted_error_type().has_value());
    EXPECT_EQ(GetParam().expected_error_type.value(),
              extracted_error_type().value());
  } else {
    EXPECT_EQ(std::nullopt, extracted_error_type());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserParentAccessObserverTest,
    testing::Values(
        TestParam({.redirect_to_target_url = true,
                   // Approval result provided and navigation completes.
                   .result_query_param = base::StringPrintf(
                       "result=%s",
                       supervised_user::CreatePacpApprovalResult()),
                   .expected_approval_result =
                       supervised_user::LocalApprovalResult::kApproved,
                   .test_name_suffix = "CompletesApprovalFlow"}),
        TestParam(
            {.redirect_to_target_url = true,
             // Approval result that can be extracted only by base64 forgiving
             // decoding.
             .result_query_param = base::StringPrintf(
                 "result=%s",
                 GetPacpApprovalResultMatchingForgivingDecoding()),
             .expected_approval_result =
                 supervised_user::LocalApprovalResult::kApproved,
             .test_name_suffix = "CompletesApprovalFlowWithForgivingDecoding"}),
        TestParam({.redirect_to_target_url = false,
                   // Approval result provide by navigation does not complete.
                   .result_query_param = base::StringPrintf(
                       "result=%s",
                       supervised_user::CreatePacpApprovalResult()),
                   .test_name_suffix = "ApprovalFlowDoesNotReachEndUrl"}),
        TestParam({.redirect_to_target_url = true,
                   // A result is provided and navigation completes,
                   // but the result does not apply to the approval flow.
                   .result_query_param = base::StringPrintf(
                       "result=%s",
                       supervised_user::CreatePacpResizeResult()),
                   .expected_approval_result =
                       supervised_user::LocalApprovalResult::kError,
                   .expected_error_type = supervised_user::
                       LocalWebApprovalErrorType::kUnexpectedPacpResponse,
                   .test_name_suffix = "FailsWithUnexpectedResult"}),
        TestParam({.redirect_to_target_url = true,
                   // A result is provided and navigation completes,
                   // but the result is in invalid encoding (Malformed result).
                   .result_query_param =
                       base::StringPrintf("result=%s",
                                          CreateInvalidEncodingResult()),
                   .expected_approval_result =
                       supervised_user::LocalApprovalResult::kError,
                   .expected_error_type = supervised_user::
                       LocalWebApprovalErrorType::kFailureToDecodePacpResponse,
                   .test_name_suffix = "FailsWithInvalidEncoding"}),
        TestParam(
            {.redirect_to_target_url = true,
             // A result query param is provided but it's empty.
             .result_query_param = "result=",
             .expected_approval_result =
                 supervised_user::LocalApprovalResult::kError,
             .expected_error_type =
                 supervised_user::LocalWebApprovalErrorType::kPacpEmptyResponse,
             .test_name_suffix = "FailsWithEmptyResult"}),
        TestParam({.redirect_to_target_url = true,
                   // A result query param is provided it's not parsed to a PACP
                   // response.
                   .result_query_param =
                       base::StringPrintf("result=%s",
                                          CreateInvalidPacpResponse()),
                   .expected_approval_result =
                       supervised_user::LocalApprovalResult::kError,
                   .expected_error_type = supervised_user::
                       LocalWebApprovalErrorType::kFailureToParsePacpResponse,
                   .test_name_suffix = "FailsWithNonParsableResponse"}),
        TestParam({.redirect_to_target_url = true,
                   // No query result provided.
                   .result_query_param = std::nullopt,
                   .test_name_suffix = "HasNoResult"})),
    [](const auto& info) { return info.param.test_name_suffix; });

}  // namespace
