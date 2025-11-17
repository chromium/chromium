// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"

// TODO(crbug.com/40249826): Refactor tests when we start emitting issues in
// bulk, via checkFormsIssues command and FormIssuesAdded event.
namespace autofill {

using testing::Eq;
using testing::Pointee;

namespace {
class AutofillFormDevtoolsProtocolTest : public DevToolsProtocolTestBase {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void NavigateToFormPageAndEnableAudits() {
    Attach();
    GURL test_url = embedded_test_server()->GetURL(
        "/autofill/autofill_form_devtools_issues_test.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
    EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

    SendCommandSync("Audits.enable");
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
                       checkFormIssuesCommandReturnsIssuesList) {
  NavigateToFormPageAndEnableAudits();
  const base::Value::Dict* res = SendCommandSync("Audits.checkFormsIssues");
  const base::Value::List* issues = res->FindListByDottedPath("formIssues");
  ASSERT_NE(issues, nullptr);
  ASSERT_EQ(issues->size(), 0ul);
}

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
  EXPECT_THAT(notification.FindByDottedPath(
                  "issue.details.genericIssueDetails.violatingNodeAttribute"),
              Pointee(Eq("id")));
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
  EXPECT_THAT(notification.FindByDottedPath(
                  "issue.details.genericIssueDetails.violatingNodeAttribute"),
              Pointee(Eq("autocomplete")));
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
      WaitForGenericIssueAdded("FormAriaLabelledByToNonExistingIdError");
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
  EXPECT_THAT(notification.FindByDottedPath(
                  "issue.details.genericIssueDetails.violatingNodeAttribute"),
              Pointee(Eq("id")));
}

IN_PROC_BROWSER_TEST_F(AutofillFormDevtoolsProtocolTest,
                       FormHasLabelWithoutNeitherForNorNestedInput) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification =
      WaitForGenericIssueAdded("FormLabelHasNeitherForNorNestedInputError");
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
  EXPECT_THAT(notification.FindByDottedPath(
                  "issue.details.genericIssueDetails.violatingNodeAttribute"),
              Pointee(Eq("for")));
}

IN_PROC_BROWSER_TEST_F(AutofillFormDevtoolsProtocolTest,
                       FormInputHasWrongButWellIntendedAutocompleteValueError) {
  NavigateToFormPageAndEnableAudits();
  base::Value::Dict notification = WaitForGenericIssueAdded(
      "FormInputHasWrongButWellIntendedAutocompleteValueError");
  EXPECT_TRUE(notification
                  .FindIntByDottedPath(
                      "issue.details.genericIssueDetails.violatingNodeId")
                  .has_value());
  EXPECT_THAT(notification.FindByDottedPath(
                  "issue.details.genericIssueDetails.violatingNodeAttribute"),
              Pointee(Eq("autocomplete")));
}

}  // namespace autofill
