// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_action.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_condition.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace helpers = extension_web_request_api_helpers;
namespace keys = extensions::declarative_webrequest_constants;

using extension_test_util::LoadManifestUnchecked;
using helpers::EventResponseDeltas;
using testing::HasSubstr;

namespace extensions {

namespace {

const char kUnknownActionType[] = "unknownType";

std::unique_ptr<WebRequestActionSet> CreateSetOfActions(const char* json) {
  base::Value::List parsed_value = base::test::ParseJsonList(json);

  base::Value::List actions;
  for (base::Value& entry : parsed_value) {
    CHECK(entry.is_dict());
    actions.Append(std::move(entry));
  }

  std::string error;
  bool bad_message = false;

  std::unique_ptr<WebRequestActionSet> action_set(WebRequestActionSet::Create(
      nullptr, nullptr, actions, &error, &bad_message));
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  CHECK(action_set);
  return action_set;
}

}  // namespace

class WebRequestActionWithThreadsTest : public ExtensionServiceTestBase {
 protected:
  void SetUp() override;

  // Creates a URL request for URL |url_string|, and applies the actions from
  // |action_set| as if they were triggered by the extension with
  // |extension_id| during |stage|.
  bool ActionWorksOnRequest(const char* url_string,
                            const std::string& extension_id,
                            const WebRequestActionSet* action_set,
                            RequestStage stage);

  // Expects a JSON description of an |action| requiring <all_urls> host
  // permission, and checks that only an extensions with full host permissions
  // can execute that action at |stage|. Also checks that the action is not
  // executable for http://clients1.google.com.
  void CheckActionNeedsAllUrls(const char* action, RequestStage stage);

  // An extension with *.com host permissions and the DWR permission.
  scoped_refptr<Extension> extension_;
  // An extension with host permissions for all URLs and the DWR permission.
  scoped_refptr<Extension> extension_all_urls_;
};

void WebRequestActionWithThreadsTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  std::string error;
  extension_ = LoadManifestUnchecked("permissions",
                                     "web_request_com_host_permissions.json",
                                     mojom::ManifestLocation::kInvalidLocation,
                                     Extension::NO_FLAGS, "ext_id_1", &error);
  ASSERT_TRUE(extension_.get()) << error;
  extension_all_urls_ = LoadManifestUnchecked(
      "permissions", "web_request_all_host_permissions.json",
      mojom::ManifestLocation::kInvalidLocation, Extension::NO_FLAGS,
      "ext_id_2", &error);
  ASSERT_TRUE(extension_all_urls_.get()) << error;
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension_);
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension_all_urls_);
}

bool WebRequestActionWithThreadsTest::ActionWorksOnRequest(
    const char* url_string,
    const std::string& extension_id,
    const WebRequestActionSet* action_set,
    RequestStage stage) {
  const int kRendererId = 2;
  EventResponseDeltas deltas;
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(""));
  WebRequestInfoInitParams params;
  params.url = GURL(url_string);
  WebRequestInfoInitParams request_params(std::move(params));
  request_params.render_process_id = kRendererId;
  WebRequestInfo request_info(std::move(request_params));
  WebRequestData request_data(&request_info, stage, headers.get());
  std::set<std::string> ignored_tags;
  WebRequestAction::ApplyInfo apply_info = {
      PermissionHelper::Get(browser_context()), raw_ref(request_data),
      false /*crosses_incognito*/, &deltas, &ignored_tags};
  action_set->Apply(extension_id, base::Time(), &apply_info);
  return (1u == deltas.size() || !ignored_tags.empty());
}

void WebRequestActionWithThreadsTest::CheckActionNeedsAllUrls(
    const char* action,
    RequestStage stage) {
  std::unique_ptr<WebRequestActionSet> action_set(CreateSetOfActions(action));

  // Although |extension_| has matching *.com host permission, |action|
  // is intentionally forbidden -- in Declarative WR, host permission
  // for less than all URLs are ignored (except in SendMessageToExtension).
  EXPECT_FALSE(ActionWorksOnRequest(
      "http://test.com", extension_->id(), action_set.get(), stage));
  // With the "<all_urls>" host permission they are allowed.
  EXPECT_TRUE(ActionWorksOnRequest(
      "http://test.com", extension_all_urls_->id(), action_set.get(), stage));

  const std::string& webstore_url =
      ExtensionsClient::Get()->GetWebstoreBaseURL().spec();
  const std::string& new_webstore_url =
      ExtensionsClient::Get()->GetNewWebstoreBaseURL().spec();
  // The protected URLs should not be touched at all.
  EXPECT_FALSE(ActionWorksOnRequest(webstore_url.c_str(), extension_->id(),
                                    action_set.get(), stage));
  EXPECT_FALSE(ActionWorksOnRequest(webstore_url.c_str(),
                                    extension_all_urls_->id(), action_set.get(),
                                    stage));
  EXPECT_FALSE(ActionWorksOnRequest(new_webstore_url.c_str(), extension_->id(),
                                    action_set.get(), stage));
  EXPECT_FALSE(ActionWorksOnRequest(new_webstore_url.c_str(),
                                    extension_all_urls_->id(), action_set.get(),
                                    stage));
}

TEST(WebRequestActionTest, CreateAction) {
  std::string error;
  bool bad_message = false;
  scoped_refptr<const WebRequestAction> result;

  // Test missing instanceType element.
  base::Value::Dict input;
  error.clear();
  result =
      WebRequestAction::Create(nullptr, nullptr, input, &error, &bad_message);
  EXPECT_TRUE(bad_message);
  EXPECT_FALSE(result.get());

  // Test wrong instanceType element.
  input.Set(keys::kInstanceTypeKey, kUnknownActionType);
  error.clear();
  result =
      WebRequestAction::Create(nullptr, nullptr, input, &error, &bad_message);
  EXPECT_NE("", error);
  EXPECT_FALSE(result.get());

  // Test success
  input.Set(keys::kInstanceTypeKey, keys::kCancelRequestType);
  error.clear();
  result =
      WebRequestAction::Create(nullptr, nullptr, input, &error, &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(WebRequestAction::ACTION_CANCEL_REQUEST, result->type());
}

TEST(WebRequestActionTest, CreateActionSet) {
  std::string error;
  bool bad_message = false;
  std::unique_ptr<WebRequestActionSet> result;

  base::Value::List input;

  // Test empty input.
  error.clear();
  result = WebRequestActionSet::Create(nullptr, nullptr, input, &error,
                                       &bad_message);
  EXPECT_TRUE(error.empty()) << error;
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_TRUE(result->actions().empty());
  EXPECT_EQ(std::numeric_limits<int>::min(), result->GetMinimumPriority());

  base::Value::Dict correct_action;
  correct_action.Set(keys::kInstanceTypeKey, keys::kIgnoreRulesType);
  correct_action.Set(keys::kLowerPriorityThanKey, 10);
  base::Value::Dict incorrect_action;
  incorrect_action.Set(keys::kInstanceTypeKey, kUnknownActionType);
  base::Value::List wrong_format_action;

  // Test success.
  input.Append(std::move(correct_action));
  error.clear();
  result = WebRequestActionSet::Create(nullptr, nullptr, input, &error,
                                       &bad_message);
  EXPECT_TRUE(error.empty()) << error;
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  ASSERT_EQ(1u, result->actions().size());
  EXPECT_EQ(WebRequestAction::ACTION_IGNORE_RULES,
            result->actions()[0]->type());
  EXPECT_EQ(10, result->GetMinimumPriority());

  // Test failure.
  input.Append(std::move(incorrect_action));
  error.clear();
  result = WebRequestActionSet::Create(nullptr, nullptr, input, &error,
                                       &bad_message);
  EXPECT_NE("", error);
  EXPECT_FALSE(result.get());

  // Test wrong data type passed.
  input.Append(std::move(wrong_format_action));
  error.clear();
  result = WebRequestActionSet::Create(nullptr, nullptr, input, &error,
                                       &bad_message);
  EXPECT_NE("", error);
  EXPECT_FALSE(result.get());
}

// Test capture group syntax conversions of WebRequestRedirectByRegExAction
TEST(WebRequestActionTest, PerlToRe2Style) {
#define CallPerlToRe2Style WebRequestRedirectByRegExAction::PerlToRe2Style
  // foo$1bar -> foo\1bar
  EXPECT_EQ("foo\\1bar", CallPerlToRe2Style("foo$1bar"));
  // foo\$1bar -> foo$1bar
  EXPECT_EQ("foo$1bar", CallPerlToRe2Style("foo\\$1bar"));
  // foo\\$1bar -> foo\\\1bar
  EXPECT_EQ("foo\\\\\\1bar", CallPerlToRe2Style("foo\\\\$1bar"));
  // foo\bar -> foobar
  EXPECT_EQ("foobar", CallPerlToRe2Style("foo\\bar"));
  // foo$bar -> foo$bar
  EXPECT_EQ("foo$bar", CallPerlToRe2Style("foo$bar"));
#undef CallPerlToRe2Style
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToRedirect) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.RedirectRequest\","
      " \"redirectUrl\": \"http://www.foobar.com\""
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_BEFORE_REQUEST);
  CheckActionNeedsAllUrls(kAction, ON_HEADERS_RECEIVED);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToRedirectByRegEx) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.RedirectByRegEx\","
      " \"from\": \".*\","
      " \"to\": \"http://www.foobar.com\""
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_BEFORE_REQUEST);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToSetRequestHeader) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.SetRequestHeader\","
      " \"name\": \"testname\","
      " \"value\": \"testvalue\""
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_BEFORE_SEND_HEADERS);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToRemoveRequestHeader) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.RemoveRequestHeader\","
      " \"name\": \"testname\""
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_BEFORE_SEND_HEADERS);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToAddResponseHeader) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.AddResponseHeader\","
      " \"name\": \"testname\","
      " \"value\": \"testvalue\""
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_HEADERS_RECEIVED);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToRemoveResponseHeader) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.RemoveResponseHeader\","
      " \"name\": \"testname\""
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_HEADERS_RECEIVED);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToSendMessageToExtension) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.SendMessageToExtension\","
      " \"message\": \"testtext\""
      "}]";
  std::unique_ptr<WebRequestActionSet> action_set(CreateSetOfActions(kAction));

  // For sending messages, specific host permissions actually matter.
  EXPECT_TRUE(ActionWorksOnRequest("http://test.com",
                                   extension_->id(),
                                   action_set.get(),
                                   ON_BEFORE_REQUEST));
  // With the "<all_urls>" host permission they are allowed.
  EXPECT_TRUE(ActionWorksOnRequest("http://test.com",
                                   extension_all_urls_->id(),
                                   action_set.get(),
                                   ON_BEFORE_REQUEST));

  // The protected URLs should not be touched at all.
  const std::string& webstore_url =
      ExtensionsClient::Get()->GetWebstoreBaseURL().spec();
  EXPECT_FALSE(ActionWorksOnRequest(webstore_url.c_str(), extension_->id(),
                                    action_set.get(), ON_BEFORE_REQUEST));
  EXPECT_FALSE(ActionWorksOnRequest(webstore_url.c_str(),
                                    extension_all_urls_->id(), action_set.get(),
                                    ON_BEFORE_REQUEST));
  const std::string& new_webstore_url =
      ExtensionsClient::Get()->GetNewWebstoreBaseURL().spec();
  EXPECT_FALSE(ActionWorksOnRequest(new_webstore_url.c_str(), extension_->id(),
                                    action_set.get(), ON_BEFORE_REQUEST));
  EXPECT_FALSE(ActionWorksOnRequest(new_webstore_url.c_str(),
                                    extension_all_urls_->id(), action_set.get(),
                                    ON_BEFORE_REQUEST));
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToAddRequestCookie) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.AddRequestCookie\","
      " \"cookie\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" }"
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_BEFORE_SEND_HEADERS);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToAddResponseCookie) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.AddResponseCookie\","
      " \"cookie\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" }"
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_HEADERS_RECEIVED);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToEditRequestCookie) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.EditRequestCookie\","
      " \"filter\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" },"
      " \"modification\": { \"name\": \"name2\", \"value\": \"value2\" }"
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_BEFORE_SEND_HEADERS);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToEditResponseCookie) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.EditResponseCookie\","
      " \"filter\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" },"
      " \"modification\": { \"name\": \"name2\", \"value\": \"value2\" }"
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_HEADERS_RECEIVED);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToRemoveRequestCookie) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.RemoveRequestCookie\","
      " \"filter\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" }"
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_BEFORE_SEND_HEADERS);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToRemoveResponseCookie) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.RemoveResponseCookie\","
      " \"filter\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" }"
      "}]";
  CheckActionNeedsAllUrls(kAction, ON_HEADERS_RECEIVED);
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToCancel) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.CancelRequest\""
      "}]";
  std::unique_ptr<WebRequestActionSet> action_set(CreateSetOfActions(kAction));

  // Cancelling requests works without full host permissions.
  EXPECT_TRUE(ActionWorksOnRequest("http://test.org",
                                   extension_->id(),
                                   action_set.get(),
                                   ON_BEFORE_REQUEST));
}

TEST_F(WebRequestActionWithThreadsTest,
       PermissionsToRedirectToTransparentImage) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.RedirectToTransparentImage\""
      "}]";
  std::unique_ptr<WebRequestActionSet> action_set(CreateSetOfActions(kAction));

  // Redirecting to transparent images works without full host permissions.
  EXPECT_TRUE(ActionWorksOnRequest("http://test.org",
                                   extension_->id(),
                                   action_set.get(),
                                   ON_BEFORE_REQUEST));
  EXPECT_TRUE(ActionWorksOnRequest("http://test.org",
                                   extension_->id(),
                                   action_set.get(),
                                   ON_HEADERS_RECEIVED));
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToRedirectToEmptyDocument) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.RedirectToEmptyDocument\""
      "}]";
  std::unique_ptr<WebRequestActionSet> action_set(CreateSetOfActions(kAction));

  // Redirecting to the empty document works without full host permissions.
  EXPECT_TRUE(ActionWorksOnRequest("http://test.org",
                                   extension_->id(),
                                   action_set.get(),
                                   ON_BEFORE_REQUEST));
  EXPECT_TRUE(ActionWorksOnRequest("http://test.org",
                                   extension_->id(),
                                   action_set.get(),
                                   ON_HEADERS_RECEIVED));
}

TEST_F(WebRequestActionWithThreadsTest, PermissionsToIgnore) {
  const char kAction[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.IgnoreRules\","
      " \"lowerPriorityThan\": 123,"
      " \"hasTag\": \"some_tag\""
      "}]";
  std::unique_ptr<WebRequestActionSet> action_set(CreateSetOfActions(kAction));

  // Ignoring rules works without full host permissions.
  EXPECT_TRUE(ActionWorksOnRequest("http://test.org",
                                   extension_->id(),
                                   action_set.get(),
                                   ON_BEFORE_REQUEST));
}

TEST(WebRequestActionTest, GetName) {
  const char kActions[] =
      "[{"
      " \"instanceType\": \"declarativeWebRequest.RedirectRequest\","
      " \"redirectUrl\": \"http://www.foobar.com\""
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.RedirectByRegEx\","
      " \"from\": \".*\","
      " \"to\": \"http://www.foobar.com\""
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.SetRequestHeader\","
      " \"name\": \"testname\","
      " \"value\": \"testvalue\""
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.RemoveRequestHeader\","
      " \"name\": \"testname\""
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.AddResponseHeader\","
      " \"name\": \"testname\","
      " \"value\": \"testvalue\""
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.RemoveResponseHeader\","
      " \"name\": \"testname\""
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.SendMessageToExtension\","
      " \"message\": \"testtext\""
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.AddRequestCookie\","
      " \"cookie\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" }"
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.AddResponseCookie\","
      " \"cookie\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" }"
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.EditRequestCookie\","
      " \"filter\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" },"
      " \"modification\": { \"name\": \"name2\", \"value\": \"value2\" }"
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.EditResponseCookie\","
      " \"filter\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" },"
      " \"modification\": { \"name\": \"name2\", \"value\": \"value2\" }"
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.RemoveRequestCookie\","
      " \"filter\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" }"
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.RemoveResponseCookie\","
      " \"filter\": { \"name\": \"cookiename\", \"value\": \"cookievalue\" }"
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.CancelRequest\""
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.RedirectToTransparentImage\""
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.RedirectToEmptyDocument\""
      "},"
      "{"
      " \"instanceType\": \"declarativeWebRequest.IgnoreRules\","
      " \"lowerPriorityThan\": 123,"
      " \"hasTag\": \"some_tag\""
      "}]";
  const char* const kExpectedNames[] = {
    "declarativeWebRequest.RedirectRequest",
    "declarativeWebRequest.RedirectByRegEx",
    "declarativeWebRequest.SetRequestHeader",
    "declarativeWebRequest.RemoveRequestHeader",
    "declarativeWebRequest.AddResponseHeader",
    "declarativeWebRequest.RemoveResponseHeader",
    "declarativeWebRequest.SendMessageToExtension",
    "declarativeWebRequest.AddRequestCookie",
    "declarativeWebRequest.AddResponseCookie",
    "declarativeWebRequest.EditRequestCookie",
    "declarativeWebRequest.EditResponseCookie",
    "declarativeWebRequest.RemoveRequestCookie",
    "declarativeWebRequest.RemoveResponseCookie",
    "declarativeWebRequest.CancelRequest",
    "declarativeWebRequest.RedirectToTransparentImage",
    "declarativeWebRequest.RedirectToEmptyDocument",
    "declarativeWebRequest.IgnoreRules",
  };
  std::unique_ptr<WebRequestActionSet> action_set(CreateSetOfActions(kActions));
  ASSERT_EQ(std::size(kExpectedNames), action_set->actions().size());
  size_t index = 0;
  for (const auto& action : action_set->actions()) {
    EXPECT_EQ(kExpectedNames[index], action->GetName());
    ++index;
  }
}

}  // namespace extensions
