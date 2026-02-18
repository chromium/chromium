// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_functional_browsertest.h"

namespace glic::test {

base::expected<base::Value, std::string> ToExpected(
    content::EvalJsResult result) {
  if (!result.is_ok()) {
    return base::unexpected(result.ExtractError());
  }
  return base::ok(std::move(result).TakeValue());
}

GlicFunctionalBrowserTestBase::GlicFunctionalBrowserTestBase() = default;
GlicFunctionalBrowserTestBase::~GlicFunctionalBrowserTestBase() = default;

content::WebContents* GlicFunctionalBrowserTestBase::web_contents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

tabs::TabInterface* GlicFunctionalBrowserTestBase::active_tab() {
  return browser()->tab_strip_model()->GetActiveTab();
}

base::expected<base::Value, std::string>
GlicFunctionalBrowserTestBase::EvalJsInGlic(const std::string_view script) {
  return ToExpected(content::EvalJs(FindGlicGuestMainFrame(), script));
}

base::expected<int, std::string>
GlicFunctionalBrowserTestBase::EvalJsInGlicForInt(
    const std::string_view script) {
  ASSIGN_OR_RETURN(base::Value js_result, EvalJsInGlic(script));
  if (std::optional<int> result = js_result.GetIfInt()) {
    return *result;
  }
  return base::unexpected("Expected an integer value from JavaScript.");
}

base::expected<std::string, std::string>
GlicFunctionalBrowserTestBase::EvalJsInGlicForString(
    const std::string_view script) {
  ASSIGN_OR_RETURN(base::Value js_result, EvalJsInGlic(script));
  if (std::string* result = js_result.GetIfString()) {
    return base::ok(*result);
  }
  return base::unexpected("Expected a string value from JavaScript.");
}

}  // namespace glic::test
