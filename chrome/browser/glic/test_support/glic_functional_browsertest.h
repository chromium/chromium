// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_FUNCTIONAL_BROWSERTEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_FUNCTIONAL_BROWSERTEST_H_

#include "base/test/gmock_expected_support.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "content/public/test/browser_test_utils.h"

namespace glic::test {

// Matches a base::expected<T, std::string> which has an error string
// that contains `expected_substring`.
MATCHER_P(ErrorHasSubstr, expected_substring, "") {
  return testing::Matches(
      base::test::ErrorIs(testing::HasSubstr(expected_substring)))(arg);
}

// Helper to convert a content::EvalJsResult to a
// base::expected<base::Value, std::string>.
base::expected<base::Value, std::string> ToExpected(
    content::EvalJsResult result);

class GlicFunctionalBrowserTestBase : public InteractiveGlicTest {
 public:
  GlicFunctionalBrowserTestBase();
  ~GlicFunctionalBrowserTestBase() override;

 protected:
  content::WebContents* web_contents();

  tabs::TabInterface* active_tab();

  // Helper to run EvalJs in the Glic frame.
  base::expected<base::Value, std::string> EvalJsInGlic(
      const std::string_view script);

  // Helper for JavaScript calls expected to return an integer value.
  base::expected<int, std::string> EvalJsInGlicForInt(
      const std::string_view script);

  // Helper for JavaScript calls expected to return a string value.
  base::expected<std::string, std::string> EvalJsInGlicForString(
      const std::string_view script);

  // Helper for JavaScript calls that return a Base64 encoded string
  // representing a serialized protobuf of type `ProtoType`.
  template <typename ProtoType>
  base::expected<ProtoType, std::string> EvalJsInGlicForBase64Proto(
      std::string_view script) {
    ASSIGN_OR_RETURN(base::Value js_result, EvalJsInGlic(script));
    const std::string* result_base64 = js_result.GetIfString();
    if (!result_base64) {
      return base::unexpected("Expected a string value from JavaScript.");
    }
    return ParseBase64Proto<ProtoType>(*result_base64);
  }
};

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_FUNCTIONAL_BROWSERTEST_H_
