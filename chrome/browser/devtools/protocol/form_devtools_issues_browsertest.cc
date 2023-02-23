// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

namespace autofill {

namespace {
class AutofillFormDevtoolsProtocolTest : public DevToolsProtocolTestBase {
 public:
  AutofillFormDevtoolsProtocolTest() {
    scoped_features_.InitAndEnableFeature(
        features::kAutofillEnableDevtoolsIssues);
  }

  void NavigateToFormPageAndEnableAudits(const std::string& url) {
    ASSERT_TRUE(embedded_test_server()->Start());
    net::EmbeddedTestServer https_test_server(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_test_server.ServeFilesFromSourceDirectory(
        "content/test/data/autofill");
    ASSERT_TRUE(https_test_server.Start());
    GURL https_url(https_test_server.GetURL(url.c_str()));

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url));
    EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

    Attach();
    SendCommandSync("Audits.enable");
  }

  void NavigateToFormPageAndEnableAudits() {
    NavigateToFormPageAndEnableAudits(
        "/autofill_form_devtools_issues_test.html");
  }

  base::Value::Dict WaitForGenericIssueAdded(const std::string& error_type) {
    auto matcher = [](const std::string& error_type,
                      const base::Value::Dict& params) {
      const std::string* maybe_error_type = params.FindStringByDottedPath(
          "issue.details.genericIssueDetails.errorType");
      return maybe_error_type && *maybe_error_type == error_type;
    };

    base::Value::Dict notification = WaitForMatchingNotification(
        "Audits.issueAdded", base::BindRepeating(matcher, error_type));

    EXPECT_EQ(*notification.FindStringByDottedPath("issue.code"),
              "GenericIssue");

    return notification;
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(AutofillFormDevtoolsProtocolTest,
                       FormHasLabelAssociatedToNameAttribute) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification =
      WaitForGenericIssueAdded("FormLabelForNameError");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(AutofillFormDevtoolsProtocolTest,
                       FormHasInputsWithDuplicateId) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification =
      WaitForGenericIssueAdded("FormDuplicateIdForInputError");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(AutofillFormDevtoolsProtocolTest,
                       FormHasInputWithNoLabels) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification =
      WaitForGenericIssueAdded("FormInputWithNoLabelError");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(AutofillFormDevtoolsProtocolTest,
                       FormHasInputWithEmptyAutocompleteAttribute) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification =
      WaitForGenericIssueAdded("FormAutocompleteAttributeEmptyError");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(AutofillFormDevtoolsProtocolTest,
                       FormHasInputWithoutIdAndName) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification =
      WaitForGenericIssueAdded("FormEmptyIdAndNameAttributesForInputError");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(
    AutofillFormDevtoolsProtocolTest,
    FormHasInputWithAriaLabelledByAttributeThatLinksToNonExistingId) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification =
      WaitForGenericIssueAdded("FormAriaLabelledByToNonExistingId");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(
    AutofillFormDevtoolsProtocolTest,
    FormHasInputAssignedAutocompleteValueToIdOrNameAttributesIssue) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification = WaitForGenericIssueAdded(
      "FormInputAssignedAutocompleteValueToIdOrNameAttributeError");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(AutofillFormDevtoolsProtocolTest,
                       FormHasLabelWithoutNeitherForNorNestedInput) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification =
      WaitForGenericIssueAdded("FormLabelHasNeitherForNorNestedInput");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(AutofillFormDevtoolsProtocolTest,
                       FormHasLabelAssociatedToNonExistingId) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification =
      WaitForGenericIssueAdded("FormLabelForMatchesNonExistingIdError");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(AutofillFormDevtoolsProtocolTest,
                       FormHasPasswordFieldWithoutUsernameFieldError) {
  NavigateToFormPageAndEnableAudits(
      "/autofill_password_form_without_username_field_devtools_issue.html");
  base::Value::Dict notification =
      WaitForGenericIssueAdded("FormHasPasswordFieldWithoutUsernameFieldError");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
}

}  // namespace autofill
