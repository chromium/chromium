// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_FLOW_TEST_UTIL_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_FLOW_TEST_UTIL_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

namespace autofill {

// Represents a JavaScript expression that evaluates to a HTMLElement.
using ElementExpr = base::StrongAlias<struct ElementExprTag, std::string>;

// A helper function for focusing a field in AutofillFlow().
// Consider using AutofillFlow() instead.
[[nodiscard]] AssertionResult FocusField(
    const ElementExpr& e,
    content::ToRenderFrameHost execution_target);

// The different ways of triggering the Autofill dropdown.
//
// A difference in their behaviour is that (only) ByArrow() opens implicitly
// selects the top-most suggestion.
struct ShowMethod {
  constexpr static ShowMethod ByArrow() { return {.arrow = true}; }
  constexpr static ShowMethod ByClick() { return {.click = true}; }
  constexpr static ShowMethod ByChar(char c) { return {.character = c}; }

  bool selects_first_suggestion() { return arrow; }

  // Exactly one of the members should be evaluate to `true`.
  const bool arrow = false;
  const char character = '\0';
  const bool click = false;
};

// We choose the timeout empirically. 250 ms are not enough; tests become flaky:
// in ShowAutofillSuggestions(), the preview triggered by an "arrow down"
// sometimes only arrives after >250 ms and thus arrives during the
// DoNothingAndWait(), which causes a crash.
constexpr base::TimeDelta kAutofillFlowDefaultTimeout = base::Seconds(2);

namespace internal {

// An Autofill consists of four stages:
// 1. focusing the field,
// 2. showing the Autofill popup,
// 3. selecting the desired suggestion,
// 4. accepting the selected suggestion.
//
// To reduce flakiness when in Stage 2, this does multiple attempts.
// Depending on the `show_method`, showing may already select the first
// suggestion; see ShowMethod for details.
//
// Selecting a profile suggestion (address or credit card) also triggers
// preview. By contrast, "Undo" and "Manage" do not cause a preview. The
// Autofill flow expects a preview for (only) the indices less than
// `num_profile_suggestions`. The selected `target_index` may be greater or
// equal to `num_profile_suggestions` to select "Undo" or "Manage".
//
// A callback can be set to be executed after each stage. Again note that
// `show_method` may select the first suggestion.
//
// The default `execution_target` is the main frame.
template <typename = void>
struct AutofillFlowParams {
  bool do_focus = true;
  bool do_show = true;
  bool do_select = true;
  bool do_accept = true;
  bool expect_previews = true;
  ShowMethod show_method = ShowMethod::ByArrow();
  int num_profile_suggestions = 1;
  int target_index = 0;
  base::RepeatingClosure after_focus = {};
  base::RepeatingClosure after_show = {};
  base::RepeatingClosure after_select = {};
  base::RepeatingClosure after_accept = {};
  size_t max_show_tries = 5;
  base::TimeDelta timeout = kAutofillFlowDefaultTimeout;
  std::optional<content::ToRenderFrameHost> execution_target = {};
};

}  // namespace internal

using AutofillFlowParams = internal::AutofillFlowParams<>;

[[nodiscard]] AssertionResult AutofillFlow(const ElementExpr& e,
                                           AutofillUiTest* test,
                                           AutofillFlowParams p = {});

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_FLOW_TEST_UTIL_H_
