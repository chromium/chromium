// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_JS_CHECKER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_JS_CHECKER_H_

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace ash {
namespace test {

class TestConditionWaiter;

using UIPath = std::initializer_list<std::string_view>;

// Utility class for tests that allows us to evaluate and check JavaScript
// expressions inside given web contents. All calls are made synchronously.
class JSChecker {
 public:
  JSChecker();
  explicit JSChecker(content::WebContents* web_contents);
  explicit JSChecker(content::RenderFrameHost* frame_host);

  // Evaluates `expression`. Evaluation will be completed when this function
  // call returns.
  void Evaluate(const std::string& expression);

  // Executes `expression`. Doesn't require a correct command. Command will be
  // queued up and executed later. This function will return immediately.
  void ExecuteAsync(const std::string& expression);

  // Evaluates `expression` and returns its result.
  [[nodiscard]] bool GetBool(const std::string& expression);
  [[nodiscard]] int GetInt(const std::string& expression);
  [[nodiscard]] std::string GetString(const std::string& expression);

  // Checks truthfulness of the given `expression`.
  void ExpectTrue(const std::string& expression);
  void ExpectFalse(const std::string& expression);

  // Compares result of `expression` with `result`.
  void ExpectEQ(const std::string& expression, int result);
  void ExpectNE(const std::string& expression, int result);
  void ExpectEQ(const std::string& expression, const std::string& result);
  void ExpectNE(const std::string& expression, const std::string& result);
  void ExpectEQ(const std::string& expression, bool result);
  void ExpectNE(const std::string& expression, bool result);

  // Evaluates value of element with `element_id`'s `attribute` and
  // returns its result.
  [[nodiscard]] bool GetAttributeBool(
      const std::string& attribute,
      std::initializer_list<std::string_view> element_id);
  [[nodiscard]] int GetAttributeInt(
      const std::string& attribute,
      std::initializer_list<std::string_view> element_id);
  [[nodiscard]] std::string GetAttributeString(
      const std::string& attribute,
      std::initializer_list<std::string_view> element_id);

  // Compares value of element with `element_id`'s `attribute` with `result`.
  void ExpectAttributeEQ(const std::string& attribute,
                         std::initializer_list<std::string_view> element_id,
                         int result);
  void ExpectAttributeNE(const std::string& attribute,
                         std::initializer_list<std::string_view> element_id,
                         int result);
  void ExpectAttributeEQ(const std::string& attribute,
                         std::initializer_list<std::string_view> element_id,
                         const std::string& result);
  void ExpectAttributeNE(const std::string& attribute,
                         std::initializer_list<std::string_view> element_id,
                         const std::string& result);
  void ExpectAttributeEQ(const std::string& attribute,
                         std::initializer_list<std::string_view> element_id,
                         bool result);
  void ExpectAttributeNE(const std::string& attribute,
                         std::initializer_list<std::string_view> element_id,
                         bool result);

  void ExpectFocused(std::initializer_list<std::string_view> element_id);
  [[nodiscard]] std::unique_ptr<TestConditionWaiter> CreateFocusWaiter(
      const std::initializer_list<std::string_view>& path);

  // Checks test waiter that would await until `js_condition` evaluates
  // to true.
  [[nodiscard]] std::unique_ptr<TestConditionWaiter> CreateWaiter(
      const std::string& js_condition);

  // Checks test waiter that would await until `js_condition` evaluates
  // to true.
  [[nodiscard]] std::unique_ptr<TestConditionWaiter>
  CreateWaiterWithDescription(const std::string& js_condition,
                              const std::string& description);

  // Waiter that waits until the given attribute is (not) present.
  // WARNING! This does not cover the case where ATTRIBUTE=false.
  // Should only be used for boolean attributes.
  [[nodiscard]] std::unique_ptr<TestConditionWaiter>
  CreateAttributePresenceWaiter(
      const std::string& attribute,
      bool presence,
      std::initializer_list<std::string_view> element_ids);

  // Waiter that waits until specified element is (not) hidden.
  [[nodiscard]] std::unique_ptr<TestConditionWaiter> CreateVisibilityWaiter(
      bool visibility,
      std::initializer_list<std::string_view> element_ids);
  [[nodiscard]] std::unique_ptr<TestConditionWaiter> CreateVisibilityWaiter(
      bool visibility,
      const std::string& element);

  // Waiter that waits until specified element is (not) displayed with non-zero
  // size.
  [[nodiscard]] std::unique_ptr<TestConditionWaiter> CreateDisplayedWaiter(
      bool displayed,
      std::initializer_list<std::string_view> element_ids);

  // Waiter that waits until an element is enabled or disabled.
  [[nodiscard]] std::unique_ptr<TestConditionWaiter> CreateEnabledWaiter(
      bool enabled,
      std::initializer_list<std::string_view> element_ids);

  // Waiter that waits until the specified element's class list contains, or
  // doesn't contain the specified class.
  [[nodiscard]] std::unique_ptr<TestConditionWaiter> CreateHasClassWaiter(
      bool has_class,
      const std::string& css_class,
      std::initializer_list<std::string_view> element_ids);

  // Waiter that waits until the specified element is (not) present in the
  // document.
  [[nodiscard]] std::unique_ptr<TestConditionWaiter>
  CreateElementTextContentWaiter(
      const std::string& content,
      std::initializer_list<std::string_view> element_ids);

  // Expects that indicated UI element is not hidden.
  // NOTE: This only checks hidden property - it might not work for elements
  // hidden by "display: none" style.
  void ExpectVisiblePath(std::initializer_list<std::string_view> element_ids);
  void ExpectVisible(const std::string& element_id);

  // Expects that indicated UI element is hidden.
  // NOTE: This only checks hidden property - it might not work for elements
  // hidden by "display: none" style.
  void ExpectHiddenPath(std::initializer_list<std::string_view> element_ids);
  void ExpectHidden(const std::string& element_id);

  // Expects that the element is displayed on screen - i.e. that it has non-null
  // size. Unlike ExpectHidden and ExpectVisible methods, this will correctly
  // elements with "display: none" style, but might not work for polymer module
  // roots.
  void ExpectPathDisplayed(bool displayed,
                           std::initializer_list<std::string_view> element_id);

  // Expects that the indicated UI element is disabled.
  void ExpectDisabledPath(std::initializer_list<std::string_view> element_ids);

  // Expects that the indicated UI element is not disabled.
  void ExpectEnabledPath(std::initializer_list<std::string_view> element_ids);

  // Expects that the indicated UI element is invalid.
  void ExpectInvalidPath(std::initializer_list<std::string_view> element_ids);

  // Expects that the indicated UI element is not invalid.
  void ExpectValidPath(std::initializer_list<std::string_view> element_ids);

  // Expects that indicated UI element has particular class.
  void ExpectHasClass(const std::string& css_class,
                      std::initializer_list<std::string_view> element_ids);
  void ExpectHasNoClass(const std::string& css_class,
                        std::initializer_list<std::string_view> element_ids);

  // Expects that indicated UI element has particular attribute.
  void ExpectHasAttribute(const std::string& attribute,
                          std::initializer_list<std::string_view> element_ids);
  void ExpectHasNoAttribute(
      const std::string& attribute,
      std::initializer_list<std::string_view> element_ids);

  // Expect that the indicated UI element has the exact same text content.
  void ExpectElementText(const std::string& content,
                         std::initializer_list<std::string_view> element_ids);

  // Expect that the indicated UI element contains particular text content.
  void ExpectElementContainsText(
      const std::string& content,
      std::initializer_list<std::string_view> element_ids);

  // Expect that the indicated UI element has the same value attribute (could be
  // used in elements like input, button, option).
  void ExpectElementValue(const std::string& value,
                          std::initializer_list<std::string_view> element_ids);

  // Expects that the indicated modal dialog is open or closed.
  void ExpectDialogOpen(std::initializer_list<std::string_view> element_ids);
  void ExpectDialogClosed(std::initializer_list<std::string_view> element_ids);

  // Fires a native 'click' event on the indicated UI element. Prefer using
  // native 'click' event as it works on both polymer and native UI elements.
  void ClickOnPath(std::initializer_list<std::string_view> element_ids);
  void ClickOn(const std::string& element_id);

  // Fires a synthetic 'tap' event on the indicated UI element. Provided as
  // backwards compatibility with some OOBE UI elements that only listen to
  // tap events.
  void TapOnPath(std::initializer_list<std::string_view> element_ids);
  void TapOnPathAsync(std::initializer_list<std::string_view> element_ids);
  void TapOn(const std::string& element_id);

  // Clicks on the indicated UI element that should be a link.
  void TapLinkOnPath(std::initializer_list<std::string_view> element_ids);

  // Select particular radio button.
  void SelectRadioPath(std::initializer_list<std::string_view> element_ids);

  // Types text into indicated input field. There is no single-element version
  // of method to avoid confusion.
  void TypeIntoPath(const std::string& value,
                    std::initializer_list<std::string_view> element_ids);

  // Selects an option in indicated <select> element. There is no single-element
  // version of method to avoid confusion.
  void SelectElementInPath(const std::string& value,
                           std::initializer_list<std::string_view> element_ids);

  bool IsVisible(std::initializer_list<std::string_view> element_ids);

  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;
};

// Helper method to create the JSChecker instance from the login/oobe
// web-contents.
JSChecker OobeJS();

// Helper method to execute the given script in the context of OOBE.
void ExecuteOobeJS(const std::string& script);
void ExecuteOobeJSAsync(const std::string& script);

// Generates JS expression that evaluates to element in hierarchy (elements
// are searched by ID in parent). It is assumed that all intermediate elements
// are Polymer-based.
std::string GetOobeElementPath(
    std::initializer_list<std::string_view> element_ids);

// Generates JS expression that evaluates to attribute of the element in
// hierarchy. It is assumed that all intermediate elements
// are Polymer-based.
std::string GetAttributeExpression(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids);

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_JS_CHECKER_H_
