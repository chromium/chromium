// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/login/test/js_checker.h"

#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/json/string_escape.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {
namespace {

bool CheckOobeCondition(content::WebContents* web_contents,
                        const std::string& js_condition) {
  return JSChecker(web_contents).GetBool(js_condition);
}

bool IsFocused(content::WebContents* web_contents,
               const std::initializer_list<std::string_view>& path) {
  if (!web_contents->GetContentNativeView()->HasFocus())
    return false;
  auto js_checker = JSChecker(web_contents);
  std::string current_active = "document.activeElement";
  for (const auto& it : path) {
    if (js_checker.GetString(current_active + ".id") != it) {
      return false;
    }
    current_active += ".shadowRoot.activeElement";
  }
  return true;
}

std::string ElementHasClassCondition(
    const std::string& css_class,
    std::initializer_list<std::string_view> element_ids) {
  std::string js = "$Element.classList.contains('$ClassName')";
  base::ReplaceSubstringsAfterOffset(&js, 0, "$ClassName", css_class);
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Element",
                                     GetOobeElementPath(element_ids));
  return js;
}

std::string ElementHasAttributeCondition(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids) {
  std::string js = "$Element.hasAttribute('$Attribute')";
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Attribute", attribute);
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Element",
                                     GetOobeElementPath(element_ids));
  return js;
}

std::string DescribePath(std::initializer_list<std::string_view> element_ids) {
  CHECK(element_ids.size() > 0);
  std::string result;
  std::initializer_list<std::string_view>::const_iterator it =
      element_ids.begin();
  result.append("//").append(std::string(*it));
  for (it++; it < element_ids.end(); it++) {
    result.append("/").append(std::string(*it));
  }
  return result;
}

}  // namespace

JSChecker::JSChecker() = default;

JSChecker::JSChecker(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

JSChecker::JSChecker(content::RenderFrameHost* frame_host) {
  web_contents_ = content::WebContents::FromRenderFrameHost(frame_host);
  CHECK(web_contents_);
}

void JSChecker::Evaluate(const std::string& expression) {
  CHECK(web_contents_);
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), expression));
}

void JSChecker::ExecuteAsync(const std::string& expression) {
  CHECK(web_contents_);
  std::string new_script = expression + ";";
  web_contents_->GetPrimaryMainFrame()
      ->ExecuteJavaScriptWithUserGestureForTests(
          base::UTF8ToUTF16(new_script), base::NullCallback(),
          content::ISOLATED_WORLD_ID_GLOBAL);
}

bool JSChecker::GetBool(const std::string& expression) {
  CHECK(web_contents_);
  return content::EvalJs(web_contents_.get(), "!!(" + expression + ")")
      .ExtractBool();
}

int JSChecker::GetInt(const std::string& expression) {
  CHECK(web_contents_);
  return content::EvalJs(web_contents_.get(), expression).ExtractInt();
}

std::string JSChecker::GetString(const std::string& expression) {
  CHECK(web_contents_);
  return content::EvalJs(web_contents_.get(), expression).ExtractString();
}

bool JSChecker::GetAttributeBool(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids) {
  return GetBool(GetAttributeExpression(attribute, element_ids));
}

int JSChecker::GetAttributeInt(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids) {
  return GetInt(GetAttributeExpression(attribute, element_ids));
}

std::string JSChecker::GetAttributeString(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids) {
  return GetString(GetAttributeExpression(attribute, element_ids));
}

void JSChecker::ExpectTrue(const std::string& expression) {
  EXPECT_TRUE(GetBool(expression)) << expression;
}

void JSChecker::ExpectFalse(const std::string& expression) {
  EXPECT_FALSE(GetBool(expression)) << expression;
}

void JSChecker::ExpectEQ(const std::string& expression, int result) {
  EXPECT_EQ(GetInt(expression), result) << expression;
}

void JSChecker::ExpectNE(const std::string& expression, int result) {
  EXPECT_NE(GetInt(expression), result) << expression;
}

void JSChecker::ExpectEQ(const std::string& expression,
                         const std::string& result) {
  EXPECT_EQ(GetString(expression), result) << expression;
}

void JSChecker::ExpectNE(const std::string& expression,
                         const std::string& result) {
  EXPECT_NE(GetString(expression), result) << expression;
}

void JSChecker::ExpectEQ(const std::string& expression, bool result) {
  EXPECT_EQ(GetBool(expression), result) << expression;
}

void JSChecker::ExpectNE(const std::string& expression, bool result) {
  EXPECT_NE(GetBool(expression), result) << expression;
}

void JSChecker::ExpectAttributeEQ(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids,
    int result) {
  ExpectEQ(GetAttributeExpression(attribute, element_ids), result);
}

void JSChecker::ExpectAttributeNE(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids,
    int result) {
  ExpectNE(GetAttributeExpression(attribute, element_ids), result);
}
void JSChecker::ExpectAttributeEQ(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids,
    const std::string& result) {
  ExpectEQ(GetAttributeExpression(attribute, element_ids), result);
}

void JSChecker::ExpectAttributeNE(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids,
    const std::string& result) {
  ExpectNE(GetAttributeExpression(attribute, element_ids), result);
}

void JSChecker::ExpectAttributeEQ(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids,
    bool result) {
  ExpectEQ(GetAttributeExpression(attribute, element_ids), result);
}

void JSChecker::ExpectAttributeNE(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids,
    bool result) {
  ExpectNE(GetAttributeExpression(attribute, element_ids), result);
}

void JSChecker::ExpectFocused(
    std::initializer_list<std::string_view> element_ids) {
  ASSERT_TRUE(web_contents_->GetContentNativeView()->HasFocus());

  std::string current_active = "document.activeElement";
  for (const auto& it : element_ids) {
    ExpectEQ(current_active + ".id", std::string(it));
    current_active += ".shadowRoot.activeElement";
  }
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateFocusWaiter(
    const std::initializer_list<std::string_view>& path) {
  auto result = std::make_unique<TestPredicateWaiter>(
      base::BindRepeating(&IsFocused, base::Unretained(web_contents_), path));
  std::string description;
  description.append(DescribePath(path)).append(" focused");
  result->set_description(description);
  return result;
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateWaiter(
    const std::string& js_condition) {
  TestPredicateWaiter::PredicateCheck predicate = base::BindRepeating(
      &CheckOobeCondition, base::Unretained(web_contents_), js_condition);
  return std::make_unique<TestPredicateWaiter>(predicate);
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateWaiterWithDescription(
    const std::string& js_condition,
    const std::string& description) {
  TestPredicateWaiter::PredicateCheck predicate = base::BindRepeating(
      &CheckOobeCondition, base::Unretained(web_contents_), js_condition);
  auto result = std::make_unique<TestPredicateWaiter>(predicate);
  result->set_description(description);
  return result;
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateAttributePresenceWaiter(
    const std::string& attribute,
    bool presence,
    std::initializer_list<std::string_view> element_ids) {
  std::string condition = ElementHasAttributeCondition(attribute, element_ids);
  if (!presence) {
    condition = "!(" + condition + ")";
  }
  std::string description;
  description.append("Attribute ")
      .append(attribute)
      .append(presence ? " present " : " absent ")
      .append("for ")
      .append(DescribePath(element_ids));
  return CreateWaiterWithDescription(condition, description);
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateVisibilityWaiter(
    bool visibility,
    std::initializer_list<std::string_view> element_ids) {
  return CreateVisibilityWaiter(visibility, GetOobeElementPath(element_ids));
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateVisibilityWaiter(
    bool visibility,
    const std::string& element) {
  std::string js_condition = element + ".hidden";
  if (visibility) {
    js_condition = "!(" + js_condition + ")";
  }
  std::string description;
  description.append(element).append(visibility ? " visible" : " hidden");
  return CreateWaiterWithDescription(js_condition, description);
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateDisplayedWaiter(
    bool displayed,
    std::initializer_list<std::string_view> element_ids) {
  const std::string element_path = GetOobeElementPath(element_ids);
  std::string js_condition = element_path + ".offsetWidth > 0 && " +
                             element_path + ".offsetHeight > 0";
  if (!displayed) {
    js_condition = "!(" + js_condition + ")";
  }
  std::string description;
  description.append(DescribePath(element_ids))
      .append(displayed ? " displayed" : " not displayed");
  return CreateWaiterWithDescription(js_condition, description);
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateEnabledWaiter(
    bool enabled,
    std::initializer_list<std::string_view> element_ids) {
  std::string js_condition = GetOobeElementPath(element_ids) + ".disabled";
  if (enabled) {
    js_condition = "!(" + js_condition + ")";
  }
  std::string description;
  description.append(DescribePath(element_ids))
      .append(enabled ? " enabled" : " disabled");
  return CreateWaiterWithDescription(js_condition, description);
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateHasClassWaiter(
    bool has_class,
    const std::string& css_class,
    std::initializer_list<std::string_view> element_ids) {
  std::string js_condition = ElementHasClassCondition(css_class, element_ids);
  if (!has_class) {
    js_condition = "!(" + js_condition + ")";
  }
  std::string description;
  description.append(DescribePath(element_ids))
      .append(" has")
      .append(has_class ? "" : " no")
      .append(" css class ")
      .append(css_class);
  return CreateWaiterWithDescription(js_condition, description);
}

std::unique_ptr<TestConditionWaiter> JSChecker::CreateElementTextContentWaiter(
    const std::string& content,
    std::initializer_list<std::string_view> element_ids) {
  TestPredicateWaiter::PredicateCheck predicate = base::BindRepeating(
      [](JSChecker* jsChecker, const std::string& content,
         std::initializer_list<std::string_view> element_ids) {
        const std::string element_text =
            jsChecker->GetAttributeString("textContent.trim()", element_ids);
        return std::string::npos != element_text.find(content);
      },
      this, content, element_ids);

  auto result = std::make_unique<TestPredicateWaiter>(predicate);

  std::string description;
  description.append(DescribePath(element_ids))
      .append(" has text content: ")
      .append(content);
  result->set_description(description);

  return result;
}

void JSChecker::ExpectVisiblePath(
    std::initializer_list<std::string_view> element_ids) {
  ExpectFalse(GetOobeElementPath(element_ids) + ".hidden");
}

void JSChecker::ExpectVisible(const std::string& element_id) {
  ExpectVisiblePath({element_id});
}

void JSChecker::ExpectHiddenPath(
    std::initializer_list<std::string_view> element_ids) {
  ExpectTrue(GetOobeElementPath(element_ids) + ".hidden");
}

void JSChecker::ExpectHidden(const std::string& element_id) {
  ExpectHiddenPath({element_id});
}

void JSChecker::ExpectPathDisplayed(
    bool displayed,
    std::initializer_list<std::string_view> element_ids) {
  const std::string element_path = GetOobeElementPath(element_ids);
  std::string js_condition = element_path + ".offsetWidth > 0 && " +
                             element_path + ".offsetHeight > 0";
  if (!displayed) {
    js_condition = "!(" + js_condition + ")";
  }
  ExpectTrue(js_condition);
}

void JSChecker::ExpectDisabledPath(
    std::initializer_list<std::string_view> element_ids) {
  ExpectAttributeEQ("disabled", element_ids, true);
}

void JSChecker::ExpectEnabledPath(
    std::initializer_list<std::string_view> element_ids) {
  ExpectAttributeEQ("disabled", element_ids, false);
}

void JSChecker::ExpectInvalidPath(
    std::initializer_list<std::string_view> element_ids) {
  ExpectAttributeEQ("invalid", element_ids, true);
}

void JSChecker::ExpectValidPath(
    std::initializer_list<std::string_view> element_ids) {
  ExpectAttributeEQ("invalid", element_ids, false);
}

void JSChecker::ExpectHasClass(
    const std::string& css_class,
    std::initializer_list<std::string_view> element_ids) {
  ExpectTrue(ElementHasClassCondition(css_class, element_ids));
}

void JSChecker::ExpectHasNoClass(
    const std::string& css_class,
    std::initializer_list<std::string_view> element_ids) {
  ExpectFalse(ElementHasClassCondition(css_class, element_ids));
}

void JSChecker::ExpectHasAttribute(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids) {
  ExpectTrue(ElementHasAttributeCondition(attribute, element_ids));
}

void JSChecker::ExpectHasNoAttribute(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids) {
  ExpectFalse(ElementHasAttributeCondition(attribute, element_ids));
}

void JSChecker::ExpectElementText(
    const std::string& content,
    std::initializer_list<std::string_view> element_ids) {
  ExpectAttributeEQ("textContent.trim()", element_ids, content);
}

void JSChecker::ExpectElementContainsText(
    const std::string& content,
    std::initializer_list<std::string_view> element_ids) {
  const std::string message =
      GetAttributeString("textContent.trim()", element_ids);
  EXPECT_TRUE(std::string::npos != message.find(content));
}

void JSChecker::ExpectDialogOpen(
    std::initializer_list<std::string_view> element_ids) {
  ExpectAttributeEQ("open", element_ids, true);
}

void JSChecker::ExpectDialogClosed(
    std::initializer_list<std::string_view> element_ids) {
  ExpectAttributeEQ("open", element_ids, false);
}

void JSChecker::ExpectElementValue(
    const std::string& value,
    std::initializer_list<std::string_view> element_ids) {
  ExpectAttributeEQ("value", element_ids, value);
}

void JSChecker::ClickOnPath(
    std::initializer_list<std::string_view> element_ids) {
  ExpectVisiblePath(element_ids);
  Evaluate(GetOobeElementPath(element_ids) + ".click()");
}

void JSChecker::ClickOn(const std::string& element_id) {
  ClickOnPath({element_id});
}

void JSChecker::TapOnPath(std::initializer_list<std::string_view> element_ids) {
  ExpectVisiblePath(element_ids);
  Evaluate(GetOobeElementPath(element_ids) + ".click()");
}

void JSChecker::TapOnPathAsync(
    std::initializer_list<std::string_view> element_ids) {
  ExpectVisiblePath(element_ids);
  ExecuteAsync(GetOobeElementPath(element_ids) + ".click()");
}

void JSChecker::TapOn(const std::string& element_id) {
  TapOnPath({element_id});
}

void JSChecker::TapLinkOnPath(
    std::initializer_list<std::string_view> element_ids) {
  ExpectVisiblePath(element_ids);
  // Make sure this method is used only on <a> html elements.
  ExpectAttributeEQ("tagName", element_ids, std::string("A"));

  Evaluate(GetOobeElementPath(element_ids) + ".click()");
}

void JSChecker::SelectRadioPath(
    std::initializer_list<std::string_view> element_ids) {
  ExpectVisiblePath(element_ids);
  // Polymer radio buttons only support click events.
  Evaluate(GetOobeElementPath(element_ids) + ".click()");
}

void JSChecker::TypeIntoPath(
    const std::string& value,
    std::initializer_list<std::string_view> element_ids) {
  ExpectVisiblePath(element_ids);
  std::string js = R"((function(){
      $FieldElem.value = '$FieldValue';
      var ie = new Event('input');
      $FieldElem.dispatchEvent(ie);
      var ce = new Event('change');
      $FieldElem.dispatchEvent(ce);
    })();)";

  std::string escaped_value;
  EXPECT_TRUE(
      base::EscapeJSONString(value, false /* put_in_quotes */, &escaped_value));

  base::ReplaceSubstringsAfterOffset(&js, 0, "$FieldElem",
                                     GetOobeElementPath(element_ids));
  base::ReplaceSubstringsAfterOffset(&js, 0, "$FieldValue", escaped_value);
  Evaluate(js);
}

void JSChecker::SelectElementInPath(
    const std::string& value,
    std::initializer_list<std::string_view> element_ids) {
  ExpectVisiblePath(element_ids);
  std::string js = R"((function(){
      $FieldElem.value = '$FieldValue';
      var ie = new Event('input');
      $FieldElem.dispatchEvent(ie);
      var ce = new Event('change');
      $FieldElem.dispatchEvent(ce);
    })();)";

  std::string escaped_value;
  EXPECT_TRUE(
      base::EscapeJSONString(value, false /* put_in_quotes */, &escaped_value));

  base::ReplaceSubstringsAfterOffset(&js, 0, "$FieldElem",
                                     GetOobeElementPath(element_ids));
  base::ReplaceSubstringsAfterOffset(&js, 0, "$FieldValue", escaped_value);
  Evaluate(js);
}

bool JSChecker::IsVisible(std::initializer_list<std::string_view> element_ids) {
  bool is_hidden = GetBool(test::GetOobeElementPath(element_ids) + ".hidden");
  return !is_hidden;
}

JSChecker OobeJS() {
  return JSChecker(LoginDisplayHost::default_host()->GetOobeWebContents());
}

void ExecuteOobeJS(const std::string& script) {
  ASSERT_TRUE(content::ExecJs(
      LoginDisplayHost::default_host()->GetOobeWebContents(), script));
}

void ExecuteOobeJSAsync(const std::string& script) {
  content::ExecuteScriptAsync(
      LoginDisplayHost::default_host()->GetOobeWebContents(), script);
}

std::string GetOobeElementPath(
    std::initializer_list<std::string_view> element_ids) {
  const char kGetElement[] = "document.getElementById('%s')";
  const char kShadowRoot[] = ".shadowRoot.querySelector('#%s')";
  CHECK(element_ids.size() > 0);
  std::initializer_list<std::string_view>::const_iterator it =
      element_ids.begin();
  auto result = base::StringPrintf(kGetElement, std::string(*it).c_str());
  for (it++; it < element_ids.end(); it++) {
      result.append(base::StringPrintf(kShadowRoot, std::string(*it).c_str()));
  }
  return result;
}

std::string GetAttributeExpression(
    const std::string& attribute,
    std::initializer_list<std::string_view> element_ids) {
  std::string result = GetOobeElementPath(element_ids);
  result.append(".");
  result.append(attribute);
  return result;
}

}  // namespace test
}  // namespace ash
