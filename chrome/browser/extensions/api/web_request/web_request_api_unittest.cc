// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/prefs/pref_member.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/web_request/upload_data_presenter.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/common/api/web_request.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-forward.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state/scoped_test_public_session_login_state.h"
#include "components/crx_file/id_util.h"
#endif

namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;
namespace web_request = extensions::api::web_request;

using base::DictionaryValue;
using base::ListValue;
using base::Time;
using base::TimeDelta;
using base::Value;
using helpers::CalculateOnAuthRequiredDelta;
using helpers::CalculateOnBeforeRequestDelta;
using helpers::CalculateOnBeforeSendHeadersDelta;
using helpers::CalculateOnHeadersReceivedDelta;
using helpers::CharListToString;
using helpers::EventResponseDelta;
using helpers::EventResponseDeltas;
using helpers::ExtraInfoSpec;
using helpers::InDecreasingExtensionInstallationTimeOrder;
using helpers::MergeCancelOfResponses;
using helpers::MergeOnBeforeRequestResponses;
using helpers::RequestCookieModification;
using helpers::ResponseCookieModification;
using helpers::ResponseHeader;
using helpers::ResponseHeaders;
using helpers::StringToCharList;
using testing::ElementsAre;
using DNRRequestAction = extensions::declarative_net_request::RequestAction;

namespace extensions {

namespace {

constexpr const char kExampleUrl[] = "http://example.com";

// Returns whether |warnings| contains an extension for |extension_id|.
bool HasIgnoredAction(const helpers::IgnoredActions& ignored_actions,
                      const std::string& extension_id,
                      web_request::IgnoredActionType action_type) {
  for (const auto& ignored_action : ignored_actions) {
    if (ignored_action.extension_id == extension_id &&
        ignored_action.action_type == action_type) {
      return true;
    }
  }
  return false;
}

}  // namespace

// A mock event router that responds to events with a pre-arranged queue of
// Tasks.
class TestIPCSender : public IPC::Sender {
 public:
  using SentMessages = std::list<std::unique_ptr<IPC::Message>>;

  // Adds a Task to the queue. We will fire these in order as events are
  // dispatched.
  void PushTask(const base::Closure& task) {
    task_queue_.push(task);
  }

  size_t GetNumTasks() { return task_queue_.size(); }

  SentMessages::const_iterator sent_begin() const {
    return sent_messages_.begin();
  }

  SentMessages::const_iterator sent_end() const {
    return sent_messages_.end();
  }

 private:
  // IPC::Sender
  bool Send(IPC::Message* message) override {
    EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_DispatchEvent::ID),
              message->type());

    EXPECT_FALSE(task_queue_.empty());
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  task_queue_.front());
    task_queue_.pop();

    sent_messages_.push_back(base::WrapUnique(message));
    return true;
  }

  base::queue<base::Closure> task_queue_;
  SentMessages sent_messages_;
};

class ExtensionWebRequestTest : public testing::Test {
 public:
  ExtensionWebRequestTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TestingProfileManager profile_manager_;
};

namespace {

// Create the numerical representation of |values|, strings passed as
// extraInfoSpec by the event handler. Returns true on success, otherwise false.
bool GenerateInfoSpec(content::BrowserContext* browser_context,
                      const std::string& values,
                      int* result) {
  // Create a base::ListValue of strings.
  base::ListValue list_value;
  for (const std::string& cur :
       base::SplitString(values, ",", base::KEEP_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY))
    list_value.AppendString(cur);
  return ExtraInfoSpec::InitFromValue(browser_context, list_value, result);
}

}  // namespace

// Tests that |render_process_id| is not relevant for adding and removing
// listeners with |web_view_instance_id| = 0.
TEST_F(ExtensionWebRequestTest, AddAndRemoveListeners) {
  std::string ext_id("abcdefghijklmnopabcdefghijklmnop");
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string kEventName(web_request::OnBeforeRequest::kEventName);
  const std::string kSubEventName = kEventName + "/1";
  EXPECT_EQ(
      0u,
      ExtensionWebRequestEventRouter::GetInstance()->GetListenerCountForTesting(
          &profile_, kEventName));

  // Add two non-webview listeners.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, ext_id, ext_id, events::FOR_TEST, kEventName, kSubEventName,
      filter, 0, 1 /* render_process_id */, 0, extensions::kMainThreadId,
      blink::mojom::kInvalidServiceWorkerVersionId);
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, ext_id, ext_id, events::FOR_TEST, kEventName, kSubEventName,
      filter, 0, 2 /* render_process_id */, 0, extensions::kMainThreadId,
      blink::mojom::kInvalidServiceWorkerVersionId);
  EXPECT_EQ(
      2u,
      ExtensionWebRequestEventRouter::GetInstance()->GetListenerCountForTesting(
          &profile_, kEventName));

  // Now remove the events without passing an explicit process ID.
  ExtensionWebRequestEventRouter::EventListener::ID id1(
      &profile_, ext_id, kSubEventName, 0, 0, extensions::kMainThreadId,
      blink::mojom::kInvalidServiceWorkerVersionId);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);
  EXPECT_EQ(
      1u,
      ExtensionWebRequestEventRouter::GetInstance()->GetListenerCountForTesting(
          &profile_, kEventName));

  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);
  EXPECT_EQ(
      0u,
      ExtensionWebRequestEventRouter::GetInstance()->GetListenerCountForTesting(
          &profile_, kEventName));
}

namespace {

void TestInitFromValue(content::BrowserContext* browser_context,
                       const std::string& values,
                       bool expected_return_code,
                       int expected_extra_info_spec) {
  int actual_info_spec;
  bool actual_return_code =
      GenerateInfoSpec(browser_context, values, &actual_info_spec);
  EXPECT_EQ(expected_return_code, actual_return_code);
  if (expected_return_code)
    EXPECT_EQ(expected_extra_info_spec, actual_info_spec);
}

}  // namespace

TEST_F(ExtensionWebRequestTest, InitFromValue) {
  TestInitFromValue(&profile_, std::string(), true, 0);

  // Single valid values.
  TestInitFromValue(&profile_, "requestHeaders", true,
                    ExtraInfoSpec::REQUEST_HEADERS);
  TestInitFromValue(&profile_, "responseHeaders", true,
                    ExtraInfoSpec::RESPONSE_HEADERS);
  TestInitFromValue(&profile_, "blocking", true, ExtraInfoSpec::BLOCKING);
  TestInitFromValue(&profile_, "asyncBlocking", true,
                    ExtraInfoSpec::ASYNC_BLOCKING);
  TestInitFromValue(&profile_, "requestBody", true,
                    ExtraInfoSpec::REQUEST_BODY);

  // Multiple valid values are bitwise-or'ed.
  TestInitFromValue(&profile_, "requestHeaders,blocking", true,
                    ExtraInfoSpec::REQUEST_HEADERS | ExtraInfoSpec::BLOCKING);

  // Any invalid values lead to a bad parse.
  TestInitFromValue(&profile_, "invalidValue", false, 0);
  TestInitFromValue(&profile_, "blocking,invalidValue", false, 0);
  TestInitFromValue(&profile_, "invalidValue1,invalidValue2", false, 0);

  // BLOCKING and ASYNC_BLOCKING are mutually exclusive.
  TestInitFromValue(&profile_, "blocking,asyncBlocking", false, 0);
}

TEST(ExtensionWebRequestHelpersTest,
     TestInDecreasingExtensionInstallationTimeOrder) {
  EventResponseDelta a("ext_1", base::Time::FromInternalValue(0));
  EventResponseDelta b("ext_2", base::Time::FromInternalValue(1000));
  EXPECT_FALSE(InDecreasingExtensionInstallationTimeOrder(a, a));
  EXPECT_FALSE(InDecreasingExtensionInstallationTimeOrder(a, b));
  EXPECT_TRUE(InDecreasingExtensionInstallationTimeOrder(b, a));
}

TEST(ExtensionWebRequestHelpersTest, TestStringToCharList) {
  base::ListValue list_value;
  list_value.AppendInteger('1');
  list_value.AppendInteger('2');
  list_value.AppendInteger('3');
  list_value.AppendInteger(0xFE);
  list_value.AppendInteger(0xD1);

  unsigned char char_value[] = {'1', '2', '3', 0xFE, 0xD1};
  std::string string_value(reinterpret_cast<char *>(char_value), 5);

  std::unique_ptr<base::ListValue> converted_list(
      StringToCharList(string_value));
  EXPECT_TRUE(list_value.Equals(converted_list.get()));

  std::string converted_string;
  EXPECT_TRUE(CharListToString(&list_value, &converted_string));
  EXPECT_EQ(string_value, converted_string);
}

TEST(ExtensionWebRequestHelpersTest, TestCalculateOnBeforeRequestDelta) {
  const bool cancel = true;
  const GURL localhost("http://localhost");
  EventResponseDelta delta = CalculateOnBeforeRequestDelta(
      "extid", base::Time::Now(), cancel, localhost);
  EXPECT_TRUE(delta.cancel);
  EXPECT_EQ(localhost, delta.new_url);
}

TEST(ExtensionWebRequestHelpersTest, TestCalculateOnBeforeSendHeadersDelta) {
  const bool cancel = true;
  std::string value;
  net::HttpRequestHeaders old_headers;
  old_headers.SetHeader("key1", "value1");
  old_headers.SetHeader("key2", "value2");

  // Test adding a header.
  net::HttpRequestHeaders new_headers_added;
  new_headers_added.SetHeader("key1", "value1");
  new_headers_added.SetHeader("key3", "value3");
  new_headers_added.SetHeader("key2", "value2");
  EventResponseDelta delta_added = CalculateOnBeforeSendHeadersDelta(
      nullptr /* browser_context */, "extid", base::Time::Now(), cancel,
      &old_headers, &new_headers_added, 0 /* extra_info_spec */);
  EXPECT_TRUE(delta_added.cancel);
  ASSERT_TRUE(delta_added.modified_request_headers.GetHeader("key3", &value));
  EXPECT_EQ("value3", value);

  // Test deleting a header.
  net::HttpRequestHeaders new_headers_deleted;
  new_headers_deleted.SetHeader("key1", "value1");
  EventResponseDelta delta_deleted = CalculateOnBeforeSendHeadersDelta(
      nullptr /* browser_context */, "extid", base::Time::Now(), cancel,
      &old_headers, &new_headers_deleted, 0 /* extra_info_spec */);
  ASSERT_EQ(1u, delta_deleted.deleted_request_headers.size());
  ASSERT_EQ("key2", delta_deleted.deleted_request_headers.front());

  // Test modifying a header.
  net::HttpRequestHeaders new_headers_modified;
  new_headers_modified.SetHeader("key1", "value1");
  new_headers_modified.SetHeader("key2", "value3");
  EventResponseDelta delta_modified = CalculateOnBeforeSendHeadersDelta(
      nullptr /* browser_context */, "extid", base::Time::Now(), cancel,
      &old_headers, &new_headers_modified, 0 /* extra_info_spec */);
  EXPECT_TRUE(delta_modified.deleted_request_headers.empty());
  ASSERT_TRUE(
      delta_modified.modified_request_headers.GetHeader("key2", &value));
  EXPECT_EQ("value3", value);

  // Test modifying a header if extension author just appended a new (key,
  // value) pair with a key that existed before. This is incorrect
  // usage of the API that shall be handled gracefully.
  net::HttpRequestHeaders new_headers_modified2;
  new_headers_modified2.SetHeader("key1", "value1");
  new_headers_modified2.SetHeader("key2", "value2");
  new_headers_modified2.SetHeader("key2", "value3");
  EventResponseDelta delta_modified2 = CalculateOnBeforeSendHeadersDelta(
      nullptr /* browser_context */, "extid", base::Time::Now(), cancel,
      &old_headers, &new_headers_modified, 0 /* extra_info_spec */);
  EXPECT_TRUE(delta_modified2.deleted_request_headers.empty());
  ASSERT_TRUE(
      delta_modified2.modified_request_headers.GetHeader("key2", &value));
  EXPECT_EQ("value3", value);
}

TEST(ExtensionWebRequestHelpersTest,
     TestCalculateOnBeforeSendHeadersDeltaWithExtraHeaders) {
  for (const std::string& name :
       {"accept-encoding", "accept-language", "cookie", "referer"}) {
    net::HttpRequestHeaders old_headers;
    old_headers.SetHeader("key1", "value1");

    // Test adding a special header.
    net::HttpRequestHeaders new_headers = old_headers;
    new_headers.SetHeader(name, "value");
    EventResponseDelta delta = CalculateOnBeforeSendHeadersDelta(
        nullptr /* browser_context */, "extid", base::Time::Now(), false,
        &old_headers, &new_headers, 0 /* extra_info_spec */);
    EXPECT_FALSE(delta.modified_request_headers.HasHeader(name));

    // Test with extra headers in spec.
    delta = CalculateOnBeforeSendHeadersDelta(
        nullptr /* browser_context */, "extid", base::Time::Now(), false,
        &old_headers, &new_headers, ExtraInfoSpec::EXTRA_HEADERS);
    std::string value;
    EXPECT_TRUE(delta.modified_request_headers.GetHeader(name, &value));
    EXPECT_EQ("value", value);

    // Test removing a special header.
    new_headers = old_headers;
    // Add header to old headers, it will be treated as removed.
    old_headers.SetHeader(name, "value");
    delta = CalculateOnBeforeSendHeadersDelta(
        nullptr /* browser_context */, "extid", base::Time::Now(), false,
        &old_headers, &new_headers, 0 /* extra_info_spec */);
    EXPECT_TRUE(delta.deleted_request_headers.empty());

    // Test with extra headers in spec.
    delta = CalculateOnBeforeSendHeadersDelta(
        nullptr /* browser_context */, "extid", base::Time::Now(), false,
        &old_headers, &new_headers, ExtraInfoSpec::EXTRA_HEADERS);
    EXPECT_THAT(delta.deleted_request_headers, ElementsAre(name));
  }
}

TEST(ExtensionWebRequestHelpersTest, TestCalculateOnHeadersReceivedDelta) {
  const bool cancel = true;
  std::string base_headers_string =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "Key2: Value2, Bar\r\n"
      "Key3: Value3\r\n"
      "Key5: Value5, end5\r\n"
      "X-Chrome-ID-Consistency-Response: Value6\r\n"
      "\r\n";
  auto base_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base_headers_string));

  ResponseHeaders new_headers = {
      {"kEy1", "Value1"},  // Unchanged
      {"Key2", "Value1"},  // Modified
      // Key3 is deleted
      {"Key4", "Value4"},                             // Added
      {"Key5", "Value5, end5"},                       // Unchanged
      {"X-Chrome-ID-Consistency-Response", "Value1"}  // Modified
  };
  GURL url;

  // The X-Chrome-ID-Consistency-Response is a protected header, but only for
  // Gaia URLs. It should be modifiable when sent from anywhere else.
  // Non-Gaia URL:
  EventResponseDelta delta = CalculateOnHeadersReceivedDelta(
      "extid", base::Time::Now(), cancel, url, url, base_headers.get(),
      &new_headers, 0 /* extra_info_spec */);
  EXPECT_TRUE(delta.cancel);
  EXPECT_THAT(
      delta.added_response_headers,
      ElementsAre(
          ResponseHeader("Key2", "Value1"), ResponseHeader("Key4", "Value4"),
          ResponseHeader("X-Chrome-ID-Consistency-Response", "Value1")));
  EXPECT_THAT(delta.deleted_response_headers,
              ElementsAre(ResponseHeader("Key2", "Value2, Bar"),
                          ResponseHeader("Key3", "Value3"),
                          ResponseHeader("X-Chrome-ID-Consistency-Response",
                                         "Value6")));

  // Gaia URL:
  delta = CalculateOnHeadersReceivedDelta(
      "extid", base::Time::Now(), cancel, GaiaUrls::GetInstance()->gaia_url(),
      url, base_headers.get(), &new_headers, 0 /* extra_info_spec */);
  EXPECT_TRUE(delta.cancel);
  EXPECT_THAT(delta.added_response_headers,
              ElementsAre(ResponseHeader("Key2", "Value1"),
                          ResponseHeader("Key4", "Value4")));
  EXPECT_THAT(delta.deleted_response_headers,
              ElementsAre(ResponseHeader("Key2", "Value2, Bar"),
                          ResponseHeader("Key3", "Value3")));
}

TEST(ExtensionWebRequestHelpersTest,
     TestCalculateOnHeadersReceivedDeltaWithExtraHeaders) {
  std::string base_headers_string =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n";
  auto base_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base_headers_string));

  ResponseHeaders new_headers = {
      {"Key1", "Value1"},
      {"Set-Cookie", "cookie"},
  };

  EventResponseDelta delta = CalculateOnHeadersReceivedDelta(
      "extid", base::Time::Now(), false, GURL(), GURL(), base_headers.get(),
      &new_headers, 0 /* extra_info_spec */);
  EXPECT_TRUE(delta.added_response_headers.empty());
  EXPECT_TRUE(delta.deleted_response_headers.empty());

  // Set-Cookie can be added if extra headers is set in options.
  delta = CalculateOnHeadersReceivedDelta(
      "extid", base::Time::Now(), false, GURL(), GURL(), base_headers.get(),
      &new_headers, ExtraInfoSpec::EXTRA_HEADERS);
  EXPECT_THAT(delta.added_response_headers,
              ElementsAre(ResponseHeader("Set-Cookie", "cookie")));
  EXPECT_TRUE(delta.deleted_response_headers.empty());

  // Test deleting Set-Cookie header.
  new_headers = {
      {"Key1", "Value1"},
  };
  base_headers->AddCookie("cookie");

  delta = CalculateOnHeadersReceivedDelta(
      "extid", base::Time::Now(), false, GURL(), GURL(), base_headers.get(),
      &new_headers, 0 /* extra_info_spec */);
  EXPECT_TRUE(delta.added_response_headers.empty());
  EXPECT_TRUE(delta.deleted_response_headers.empty());

  delta = CalculateOnHeadersReceivedDelta(
      "extid", base::Time::Now(), false, GURL(), GURL(), base_headers.get(),
      &new_headers, ExtraInfoSpec::EXTRA_HEADERS);
  EXPECT_TRUE(delta.added_response_headers.empty());
  EXPECT_THAT(delta.deleted_response_headers,
              ElementsAre(ResponseHeader("Set-Cookie", "cookie")));
}

TEST(ExtensionWebRequestHelpersTest, TestCalculateOnAuthRequiredDelta) {
  const bool cancel = true;

  base::string16 username = base::ASCIIToUTF16("foo");
  base::string16 password = base::ASCIIToUTF16("bar");
  net::AuthCredentials credentials(username, password);

  EventResponseDelta delta = CalculateOnAuthRequiredDelta(
      "extid", base::Time::Now(), cancel, credentials);
  EXPECT_TRUE(delta.cancel);
  ASSERT_TRUE(delta.auth_credentials.has_value());
  EXPECT_EQ(username, delta.auth_credentials->username());
  EXPECT_EQ(password, delta.auth_credentials->password());
}

TEST(ExtensionWebRequestHelpersTest, TestMergeCancelOfResponses) {
  EventResponseDeltas deltas;
  bool canceled = false;

  // Single event that does not cancel.
  {
    EventResponseDelta d1("extid1", base::Time::FromInternalValue(1000));
    d1.cancel = false;
    deltas.push_back(std::move(d1));
  }
  MergeCancelOfResponses(deltas, &canceled);
  EXPECT_FALSE(canceled);

  // Second event that cancels the request
  {
    EventResponseDelta d2("extid2", base::Time::FromInternalValue(500));
    d2.cancel = true;
    deltas.push_back(std::move(d2));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  MergeCancelOfResponses(deltas, &canceled);
  EXPECT_TRUE(canceled);
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses) {
  EventResponseDeltas deltas;
  helpers::IgnoredActions ignored_actions;
  GURL effective_new_url;

  // No redirect
  {
    EventResponseDelta d0("extid0", base::Time::FromInternalValue(0));
    deltas.push_back(std::move(d0));
  }
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_TRUE(effective_new_url.is_empty());

  // Single redirect.
  GURL new_url_1("http://foo.com");
  {
    EventResponseDelta d1("extid1", base::Time::FromInternalValue(1000));
    d1.new_url = GURL(new_url_1);
    deltas.push_back(std::move(d1));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(ignored_actions.empty());

  // Ignored redirect (due to precedence).
  GURL new_url_2("http://bar.com");
  {
    EventResponseDelta d2("extid2", base::Time::FromInternalValue(500));
    d2.new_url = GURL(new_url_2);
    deltas.push_back(std::move(d2));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid2",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));

  // Overriding redirect.
  GURL new_url_3("http://baz.com");
  {
    EventResponseDelta d3("extid3", base::Time::FromInternalValue(1500));
    d3.new_url = GURL(new_url_3);
    deltas.push_back(std::move(d3));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_EQ(new_url_3, effective_new_url);
  EXPECT_EQ(2u, ignored_actions.size());
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid1",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid2",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));

  // Check that identical redirects don't cause a conflict.
  {
    EventResponseDelta d4("extid4", base::Time::FromInternalValue(2000));
    d4.new_url = GURL(new_url_3);
    deltas.push_back(std::move(d4));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_EQ(new_url_3, effective_new_url);
  EXPECT_EQ(2u, ignored_actions.size());
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid1",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid2",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));
}

// This tests that we can redirect to data:// urls, which is considered
// a kind of cancelling requests.
TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses2) {
  EventResponseDeltas deltas;
  helpers::IgnoredActions ignored_actions;
  GURL effective_new_url;

  // Single redirect.
  GURL new_url_0("http://foo.com");
  {
    EventResponseDelta d0("extid0", base::Time::FromInternalValue(2000));
    d0.new_url = GURL(new_url_0);
    deltas.push_back(std::move(d0));
  }
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_EQ(new_url_0, effective_new_url);

  // Cancel request by redirecting to a data:// URL. This shall override
  // the other redirect but not cause any conflict warnings.
  GURL new_url_1("data://foo");
  {
    EventResponseDelta d1("extid1", base::Time::FromInternalValue(1500));
    d1.new_url = GURL(new_url_1);
    deltas.push_back(std::move(d1));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(ignored_actions.empty());

  // Cancel request by redirecting to the same data:// URL. This shall
  // not create any conflicts as it is in line with d1.
  GURL new_url_2("data://foo");
  {
    EventResponseDelta d2("extid2", base::Time::FromInternalValue(1000));
    d2.new_url = GURL(new_url_2);
    deltas.push_back(std::move(d2));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();

  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(ignored_actions.empty());

  // Cancel redirect by redirecting to a different data:// URL. This needs
  // to create a conflict.
  GURL new_url_3("data://something_totally_different");
  {
    EventResponseDelta d3("extid3", base::Time::FromInternalValue(500));
    d3.new_url = GURL(new_url_3);
    deltas.push_back(std::move(d3));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid3",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));
}

// This tests that we can redirect to about:blank, which is considered
// a kind of cancelling requests.
TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses3) {
  EventResponseDeltas deltas;
  helpers::IgnoredActions ignored_actions;
  GURL effective_new_url;

  // Single redirect.
  GURL new_url_0("http://foo.com");
  {
    EventResponseDelta d0("extid0", base::Time::FromInternalValue(2000));
    d0.new_url = GURL(new_url_0);
    deltas.push_back(std::move(d0));
  }
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_EQ(new_url_0, effective_new_url);

  // Cancel request by redirecting to about:blank. This shall override
  // the other redirect but not cause any conflict warnings.
  GURL new_url_1("about:blank");
  {
    EventResponseDelta d1("extid1", base::Time::FromInternalValue(1500));
    d1.new_url = GURL(new_url_1);
    deltas.push_back(std::move(d1));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(ignored_actions.empty());
}

// This tests that WebSocket requests can not be redirected.
TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses4) {
  EventResponseDeltas deltas;
  helpers::IgnoredActions ignored_actions;
  GURL effective_new_url;

  // Single redirect.
  {
    EventResponseDelta delta("extid", base::Time::FromInternalValue(2000));
    delta.new_url = GURL("http://foo.com");
    deltas.push_back(std::move(delta));
  }
  MergeOnBeforeRequestResponses(GURL("ws://example.com"), deltas,
                                &effective_new_url, &ignored_actions);
  EXPECT_EQ(GURL(), effective_new_url);
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeSendHeadersResponses) {
  net::HttpRequestHeaders base_headers;
  base_headers.SetHeader("key1", "value 1");
  base_headers.SetHeader("key2", "value 2");
  helpers::IgnoredActions ignored_actions;
  std::string header_value;
  EventResponseDeltas deltas;

  // Check that we can handle not changing the headers.
  {
    EventResponseDelta d0("extid0", base::Time::FromInternalValue(2500));
    deltas.push_back(std::move(d0));
  }
  bool request_headers_modified0;
  std::set<std::string> ignore1, ignore2;
  net::HttpRequestHeaders headers0;
  headers0.MergeFrom(base_headers);
  WebRequestInfoInitParams info_params;
  WebRequestInfo info(std::move(info_params));
  info.dnr_actions = std::vector<DNRRequestAction>();
  MergeOnBeforeSendHeadersResponses(info, deltas, &headers0, &ignored_actions,
                                    &ignore1, &ignore2,
                                    &request_headers_modified0);
  ASSERT_TRUE(headers0.GetHeader("key1", &header_value));
  EXPECT_EQ("value 1", header_value);
  ASSERT_TRUE(headers0.GetHeader("key2", &header_value));
  EXPECT_EQ("value 2", header_value);
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_FALSE(request_headers_modified0);

  // Delete, modify and add a header.
  {
    EventResponseDelta d1("extid1", base::Time::FromInternalValue(2000));
    d1.deleted_request_headers.push_back("key1");
    d1.modified_request_headers.SetHeader("key2", "value 3");
    d1.modified_request_headers.SetHeader("key3", "value 3");
    deltas.push_back(std::move(d1));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  ignore1.clear();
  ignore2.clear();
  bool request_headers_modified1;
  net::HttpRequestHeaders headers1;
  headers1.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(info, deltas, &headers1, &ignored_actions,
                                    &ignore1, &ignore2,
                                    &request_headers_modified1);
  EXPECT_FALSE(headers1.HasHeader("key1"));
  ASSERT_TRUE(headers1.GetHeader("key2", &header_value));
  EXPECT_EQ("value 3", header_value);
  ASSERT_TRUE(headers1.GetHeader("key3", &header_value));
  EXPECT_EQ("value 3", header_value);
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_TRUE(request_headers_modified1);

  // Check that conflicts are atomic, i.e. if one header modification
  // collides all other conflicts of the same extension are declined as well.
  {
    EventResponseDelta d2("extid2", base::Time::FromInternalValue(1500));
    // This one conflicts:
    d2.modified_request_headers.SetHeader("key3", "value 0");
    d2.modified_request_headers.SetHeader("key4", "value 4");
    deltas.push_back(std::move(d2));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  ignore1.clear();
  ignore2.clear();
  bool request_headers_modified2;
  net::HttpRequestHeaders headers2;
  headers2.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(info, deltas, &headers2, &ignored_actions,
                                    &ignore1, &ignore2,
                                    &request_headers_modified2);
  EXPECT_FALSE(headers2.HasHeader("key1"));
  ASSERT_TRUE(headers2.GetHeader("key2", &header_value));
  EXPECT_EQ("value 3", header_value);
  ASSERT_TRUE(headers2.GetHeader("key3", &header_value));
  EXPECT_EQ("value 3", header_value);
  EXPECT_FALSE(headers2.HasHeader("key4"));
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid2",
                       web_request::IGNORED_ACTION_TYPE_REQUEST_HEADERS));
  EXPECT_TRUE(request_headers_modified2);

  // Check that identical modifications don't conflict and operations
  // can be merged.
  {
    EventResponseDelta d3("extid3", base::Time::FromInternalValue(1000));
    d3.deleted_request_headers.push_back("key1");
    d3.modified_request_headers.SetHeader("key2", "value 3");
    d3.modified_request_headers.SetHeader("key5", "value 5");
    deltas.push_back(std::move(d3));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  ignore1.clear();
  ignore2.clear();
  bool request_headers_modified3;
  net::HttpRequestHeaders headers3;
  headers3.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(info, deltas, &headers3, &ignored_actions,
                                    &ignore1, &ignore2,
                                    &request_headers_modified3);
  EXPECT_FALSE(headers3.HasHeader("key1"));
  ASSERT_TRUE(headers3.GetHeader("key2", &header_value));
  EXPECT_EQ("value 3", header_value);
  ASSERT_TRUE(headers3.GetHeader("key3", &header_value));
  EXPECT_EQ("value 3", header_value);
  ASSERT_TRUE(headers3.GetHeader("key5", &header_value));
  EXPECT_EQ("value 5", header_value);
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid2",
                       web_request::IGNORED_ACTION_TYPE_REQUEST_HEADERS));
  EXPECT_TRUE(request_headers_modified3);

  // Check that headers removed by Declarative Net Request API can't be modified
  // and result in a conflict.
  ignored_actions.clear();
  ignore1.clear();
  ignore2.clear();
  bool request_headers_modified4 = false;
  net::HttpRequestHeaders headers4;
  headers4.MergeFrom(base_headers);

  DNRRequestAction remove_headers_action =
      CreateRequestActionForTesting(DNRRequestAction::Type::REMOVE_HEADERS);
  remove_headers_action.request_headers_to_remove = {"key5"};
  info.dnr_actions = std::vector<DNRRequestAction>();
  info.dnr_actions->push_back(std::move(remove_headers_action));
  MergeOnBeforeSendHeadersResponses(info, deltas, &headers4, &ignored_actions,
                                    &ignore1, &ignore2,
                                    &request_headers_modified4);
  // deleted by |d1|.
  EXPECT_FALSE(headers4.HasHeader("key1"));
  // Added by |d1|.
  ASSERT_TRUE(headers4.GetHeader("key2", &header_value));
  EXPECT_EQ("value 3", header_value);
  // Removed by Declarative Net Request API.
  EXPECT_FALSE(headers4.HasHeader("key5"));
  EXPECT_EQ(2u, ignored_actions.size());
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid2",
                       web_request::IGNORED_ACTION_TYPE_REQUEST_HEADERS));
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid2",
                       web_request::IGNORED_ACTION_TYPE_REQUEST_HEADERS));
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid3",
                       web_request::IGNORED_ACTION_TYPE_REQUEST_HEADERS));
  EXPECT_TRUE(request_headers_modified4);
}

// Ensure conflicts between different extensions are handled correctly with
// header names being interpreted in a case insensitive manner. Regression test
// for crbug.com/956795.
TEST(ExtensionWebRequestHelpersTest,
     TestMergeOnBeforeSendHeadersResponses_Conflicts) {
  // Have two extensions which both modify header "key1".
  EventResponseDeltas deltas;
  {
    EventResponseDelta d1("extid1", base::Time::FromInternalValue(2000));
    d1.modified_request_headers.SetHeader("key1", "ext1");
    deltas.push_back(std::move(d1));
  }

  {
    EventResponseDelta d2("extid2", base::Time::FromInternalValue(1000));
    d2.modified_request_headers.SetHeader("KEY1", "ext2");
    deltas.push_back(std::move(d2));
  }

  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);

  WebRequestInfoInitParams info_params;
  WebRequestInfo info(std::move(info_params));
  info.dnr_actions = std::vector<DNRRequestAction>();
  helpers::IgnoredActions ignored_actions;
  std::set<std::string> removed_headers, set_headers;
  bool request_headers_modified = false;

  net::HttpRequestHeaders headers;
  headers.SetHeader("key1", "value 1");

  MergeOnBeforeSendHeadersResponses(info, deltas, &headers, &ignored_actions,
                                    &removed_headers, &set_headers,
                                    &request_headers_modified);

  std::string header_value;
  ASSERT_TRUE(headers.GetHeader("key1", &header_value));
  EXPECT_EQ("ext1", header_value);
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(request_headers_modified);
  EXPECT_THAT(removed_headers, ::testing::IsEmpty());
  EXPECT_THAT(set_headers, ElementsAre("key1"));
}

TEST(ExtensionWebRequestHelpersTest,
     TestMergeOnBeforeSendHeadersResponses_Cookies) {
  net::HttpRequestHeaders base_headers;
  base_headers.SetHeader("Cookie",
                         "name=value; name2=value2; name3=\"value3\"");
  helpers::IgnoredActions ignored_actions;
  std::string header_value;
  EventResponseDeltas deltas;

  RequestCookieModification add_cookie;
  add_cookie.type = helpers::ADD;
  add_cookie.modification.emplace();
  add_cookie.modification->name = "name4";
  add_cookie.modification->value = "\"value 4\"";

  RequestCookieModification add_cookie_2;
  add_cookie_2.type = helpers::ADD;
  add_cookie_2.modification.emplace();
  add_cookie_2.modification->name = "name";
  add_cookie_2.modification->value = "new value";

  RequestCookieModification edit_cookie;
  edit_cookie.type = helpers::EDIT;
  edit_cookie.filter.emplace();
  edit_cookie.filter->name = "name2";
  edit_cookie.modification.emplace();
  edit_cookie.modification->value = "new value";

  RequestCookieModification remove_cookie;
  remove_cookie.type = helpers::REMOVE;
  remove_cookie.filter.emplace();
  remove_cookie.filter->name = "name3";

  RequestCookieModification* operations[] = {&add_cookie, &add_cookie_2,
                                             &edit_cookie, &remove_cookie};

  int64_t time = 0;
  for (auto* operation : operations) {
    EventResponseDelta delta("extid0",
                             base::Time::FromInternalValue(time++ * 5));
    delta.request_cookie_modifications.push_back(std::move(*operation));
    deltas.push_back(std::move(delta));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  bool request_headers_modified1;
  std::set<std::string> ignore1, ignore2;
  net::HttpRequestHeaders headers1;
  headers1.MergeFrom(base_headers);
  ignored_actions.clear();

  WebRequestInfoInitParams info_params;
  WebRequestInfo info(std::move(info_params));
  MergeOnBeforeSendHeadersResponses(info, deltas, &headers1, &ignored_actions,
                                    &ignore1, &ignore2,
                                    &request_headers_modified1);
  EXPECT_TRUE(headers1.HasHeader("Cookie"));
  ASSERT_TRUE(headers1.GetHeader("Cookie", &header_value));
  EXPECT_EQ("name=new value; name2=new value; name4=\"value 4\"", header_value);
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_FALSE(request_headers_modified1);
}

namespace {

std::string GetCookieExpirationDate(int delta_secs) {
  const char* const kWeekDays[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };
  const char* const kMonthNames[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  Time::Exploded exploded_time;
  (Time::Now() + TimeDelta::FromSeconds(delta_secs)).UTCExplode(&exploded_time);

  return base::StringPrintf("%s, %d %s %d %.2d:%.2d:%.2d GMT",
                            kWeekDays[exploded_time.day_of_week],
                            exploded_time.day_of_month,
                            kMonthNames[exploded_time.month - 1],
                            exploded_time.year,
                            exploded_time.hour,
                            exploded_time.minute,
                            exploded_time.second);
}

}  // namespace

TEST(ExtensionWebRequestHelpersTest,
     TestMergeCookiesInOnHeadersReceivedResponses) {
  std::string header_value;
  EventResponseDeltas deltas;

  std::string cookie_expiration = GetCookieExpirationDate(1200);
  std::string base_headers_string =
      "HTTP/1.0 200 OK\r\n"
      "Foo: Bar\r\n"
      "Set-Cookie: name=value; DOMAIN=google.com; Secure\r\n"
      "Set-Cookie: name2=value2\r\n"
      "Set-Cookie: name3=value3\r\n"
      "Set-Cookie: lBound1=value5; Expires=" + cookie_expiration + "\r\n"
      "Set-Cookie: lBound2=value6; Max-Age=1200\r\n"
      "Set-Cookie: lBound3=value7; Max-Age=2000\r\n"
      "Set-Cookie: uBound1=value8; Expires=" + cookie_expiration + "\r\n"
      "Set-Cookie: uBound2=value9; Max-Age=1200\r\n"
      "Set-Cookie: uBound3=value10; Max-Age=2000\r\n"
      "Set-Cookie: uBound4=value11; Max-Age=2500\r\n"
      "Set-Cookie: uBound5=value12; Max-Age=600; Expires=" +
      cookie_expiration + "\r\n"
      "Set-Cookie: uBound6=removed; Max-Age=600\r\n"
      "Set-Cookie: sessionCookie=removed; Max-Age=INVALID\r\n"
      "Set-Cookie: sessionCookie2=removed\r\n"
      "\r\n";
  auto base_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base_headers_string));

  // Check that we can handle if not touching the response headers.
  {
    EventResponseDelta d0("extid0", base::Time::FromInternalValue(3000));
    deltas.push_back(std::move(d0));
  }
  scoped_refptr<net::HttpResponseHeaders> new_headers0;
  MergeCookiesInOnHeadersReceivedResponses(GURL(), deltas, base_headers.get(),
                                           &new_headers0);
  EXPECT_FALSE(new_headers0.get());

  ResponseCookieModification add_cookie;
  add_cookie.type = helpers::ADD;
  add_cookie.modification.emplace();
  add_cookie.modification->name = "name4";
  add_cookie.modification->value = "\"value4\"";

  ResponseCookieModification edit_cookie;
  edit_cookie.type = helpers::EDIT;
  edit_cookie.filter.emplace();
  edit_cookie.filter->name = "name2";
  edit_cookie.modification.emplace();
  edit_cookie.modification->value = "new value";

  ResponseCookieModification edit_cookie_2;
  edit_cookie_2.type = helpers::EDIT;
  edit_cookie_2.filter.emplace();
  edit_cookie_2.filter->secure = false;
  edit_cookie_2.modification.emplace();
  edit_cookie_2.modification->secure = true;

  // Tests 'ageLowerBound' filter when cookie lifetime is set
  // in cookie's 'max-age' attribute and its value is greater than
  // the filter's value.
  ResponseCookieModification edit_cookie_3;
  edit_cookie_3.type = helpers::EDIT;
  edit_cookie_3.filter.emplace();
  edit_cookie_3.filter->name = "lBound1";
  edit_cookie_3.filter->age_lower_bound = 600;
  edit_cookie_3.modification.emplace();
  edit_cookie_3.modification->value = "greater_1";

  // Cookie lifetime is set in the cookie's 'expires' attribute.
  ResponseCookieModification edit_cookie_4;
  edit_cookie_4.type = helpers::EDIT;
  edit_cookie_4.filter.emplace();
  edit_cookie_4.filter->name = "lBound2";
  edit_cookie_4.filter->age_lower_bound = 600;
  edit_cookie_4.modification.emplace();
  edit_cookie_4.modification->value = "greater_2";

  // Tests equality of the cookie lifetime with the filter value when
  // lifetime is set in the cookie's 'max-age' attribute.
  // Note: we don't test the equality when the lifetime is set in the 'expires'
  // attribute because the tests will be flaky. The reason is calculations will
  // depend on fetching the current time.
  ResponseCookieModification edit_cookie_5;
  edit_cookie_5.type = helpers::EDIT;
  edit_cookie_5.filter.emplace();
  edit_cookie_5.filter->name = "lBound3";
  edit_cookie_5.filter->age_lower_bound = 2000;
  edit_cookie_5.modification.emplace();
  edit_cookie_5.modification->value = "equal_2";

  // Tests 'ageUpperBound' filter when cookie lifetime is set
  // in cookie's 'max-age' attribute and its value is lower than
  // the filter's value.
  ResponseCookieModification edit_cookie_6;
  edit_cookie_6.type = helpers::EDIT;
  edit_cookie_6.filter.emplace();
  edit_cookie_6.filter->name = "uBound1";
  edit_cookie_6.filter->age_upper_bound = 2000;
  edit_cookie_6.modification.emplace();
  edit_cookie_6.modification->value = "smaller_1";

  // Cookie lifetime is set in the cookie's 'expires' attribute.
  ResponseCookieModification edit_cookie_7;
  edit_cookie_7.type = helpers::EDIT;
  edit_cookie_7.filter.emplace();
  edit_cookie_7.filter->name = "uBound2";
  edit_cookie_7.filter->age_upper_bound = 2000;
  edit_cookie_7.modification.emplace();
  edit_cookie_7.modification->value = "smaller_2";

  // Tests equality of the cookie lifetime with the filter value when
  // lifetime is set in the cookie's 'max-age' attribute.
  ResponseCookieModification edit_cookie_8;
  edit_cookie_8.type = helpers::EDIT;
  edit_cookie_8.filter.emplace();
  edit_cookie_8.filter->name = "uBound3";
  edit_cookie_8.filter->age_upper_bound = 2000;
  edit_cookie_8.modification.emplace();
  edit_cookie_8.modification->value = "equal_4";

  // Tests 'ageUpperBound' filter when cookie lifetime is greater
  // than the filter value. No modification is expected to be applied.
  ResponseCookieModification edit_cookie_9;
  edit_cookie_9.type = helpers::EDIT;
  edit_cookie_9.filter.emplace();
  edit_cookie_9.filter->name = "uBound4";
  edit_cookie_9.filter->age_upper_bound = 2501;
  edit_cookie_9.modification.emplace();
  edit_cookie_9.modification->value = "Will not change";

  // Tests 'ageUpperBound' filter when both 'max-age' and 'expires' cookie
  // attributes are provided. 'expires' value matches the filter, however
  // no modification to the cookie is expected because 'max-age' overrides
  // 'expires' and it does not match the filter.
  ResponseCookieModification edit_cookie_10;
  edit_cookie_10.type = helpers::EDIT;
  edit_cookie_10.filter.emplace();
  edit_cookie_10.filter->name = "uBound5";
  edit_cookie_10.filter->age_upper_bound = 800;
  edit_cookie_10.modification.emplace();
  edit_cookie_10.modification->value = "Will not change";

  ResponseCookieModification remove_cookie;
  remove_cookie.type = helpers::REMOVE;
  remove_cookie.filter.emplace();
  remove_cookie.filter->name = "name3";

  ResponseCookieModification remove_cookie_2;
  remove_cookie_2.type = helpers::REMOVE;
  remove_cookie_2.filter.emplace();
  remove_cookie_2.filter->name = "uBound6";
  remove_cookie_2.filter->age_upper_bound = 700;

  ResponseCookieModification remove_cookie_3;
  remove_cookie_3.type = helpers::REMOVE;
  remove_cookie_3.filter.emplace();
  remove_cookie_3.filter->name = "sessionCookie";
  remove_cookie_3.filter->session_cookie = true;

  ResponseCookieModification remove_cookie_4;
  remove_cookie_4.type = helpers::REMOVE;
  remove_cookie_4.filter.emplace();
  remove_cookie_4.filter->name = "sessionCookie2";
  remove_cookie_4.filter->session_cookie = true;

  ResponseCookieModification* operations[] = {
      &add_cookie,      &edit_cookie,     &edit_cookie_2,  &edit_cookie_3,
      &edit_cookie_4,   &edit_cookie_5,   &edit_cookie_6,  &edit_cookie_7,
      &edit_cookie_8,   &edit_cookie_9,   &edit_cookie_10, &remove_cookie,
      &remove_cookie_2, &remove_cookie_3, &remove_cookie_4};

  int64_t time = 0;
  for (auto* operation : operations) {
    EventResponseDelta delta("extid0",
                             base::Time::FromInternalValue(time++ * 5));
    delta.response_cookie_modifications.push_back(std::move(*operation));
    deltas.push_back(std::move(delta));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  auto headers1 = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base_headers_string));
  scoped_refptr<net::HttpResponseHeaders> new_headers1;
  MergeCookiesInOnHeadersReceivedResponses(GURL(), deltas, headers1.get(),
                                           &new_headers1);

  EXPECT_TRUE(new_headers1->HasHeader("Foo"));
  size_t iter = 0;
  std::string cookie_string;
  std::set<std::string> expected_cookies;
  expected_cookies.insert("name=value; domain=google.com; secure");
  expected_cookies.insert("name2=value2; secure");
  expected_cookies.insert("name4=\"value4\"; secure");
  expected_cookies.insert(
      "lBound1=greater_1; expires=" + cookie_expiration + "; secure");
  expected_cookies.insert("lBound2=greater_2; max-age=1200; secure");
  expected_cookies.insert("lBound3=equal_2; max-age=2000; secure");
  expected_cookies.insert(
      "uBound1=smaller_1; expires=" + cookie_expiration + "; secure");
  expected_cookies.insert("uBound2=smaller_2; max-age=1200; secure");
  expected_cookies.insert("uBound3=equal_4; max-age=2000; secure");
  expected_cookies.insert("uBound4=value11; max-age=2500; secure");
  expected_cookies.insert(
      "uBound5=value12; max-age=600; expires=" + cookie_expiration+ "; secure");
  std::set<std::string> actual_cookies;
  while (new_headers1->EnumerateHeader(&iter, "Set-Cookie", &cookie_string))
    actual_cookies.insert(cookie_string);
  EXPECT_EQ(expected_cookies, actual_cookies);
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnHeadersReceivedResponses) {
  helpers::IgnoredActions ignored_actions;
  std::string header_value;
  EventResponseDeltas deltas;

  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "Key2: Value2, Foo\r\n"
      "\r\n";
  auto base_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base_headers_string));

  // Check that we can handle if not touching the response headers.
  {
    EventResponseDelta d0("extid0", base::Time::FromInternalValue(3000));
    deltas.push_back(std::move(d0));
  }
  bool response_headers_modified0;
  scoped_refptr<net::HttpResponseHeaders> new_headers0;
  GURL preserve_fragment_on_redirect_url0;
  WebRequestInfoInitParams info_params;
  info_params.url = GURL(kExampleUrl);
  WebRequestInfo info(std::move(info_params));
  info.dnr_actions = std::vector<DNRRequestAction>();

  MergeOnHeadersReceivedResponses(
      info, deltas, base_headers.get(), &new_headers0,
      &preserve_fragment_on_redirect_url0, &ignored_actions,
      &response_headers_modified0);
  EXPECT_FALSE(new_headers0.get());
  EXPECT_TRUE(preserve_fragment_on_redirect_url0.is_empty());
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_FALSE(response_headers_modified0);

  {
    EventResponseDelta d1("extid1", base::Time::FromInternalValue(2000));
    d1.deleted_response_headers.push_back(ResponseHeader("KEY1", "Value1"));
    d1.deleted_response_headers.push_back(
        ResponseHeader("KEY2", "Value2, Foo"));
    d1.added_response_headers.push_back(ResponseHeader("Key2", "Value3"));
    d1.added_response_headers.push_back(ResponseHeader("Key3", "Foo"));
    deltas.push_back(std::move(d1));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  bool response_headers_modified1;
  scoped_refptr<net::HttpResponseHeaders> new_headers1;
  GURL preserve_fragment_on_redirect_url1;
  MergeOnHeadersReceivedResponses(
      info, deltas, base_headers.get(), &new_headers1,
      &preserve_fragment_on_redirect_url1, &ignored_actions,
      &response_headers_modified1);
  ASSERT_TRUE(new_headers1.get());
  EXPECT_TRUE(preserve_fragment_on_redirect_url1.is_empty());
  std::multimap<std::string, std::string> expected1;
  expected1.insert(std::pair<std::string, std::string>("Key2", "Value3"));
  expected1.insert(std::pair<std::string, std::string>("Key3", "Foo"));
  size_t iter = 0;
  std::string name;
  std::string value;
  std::multimap<std::string, std::string> actual1;
  while (new_headers1->EnumerateHeaderLines(&iter, &name, &value)) {
    actual1.insert(std::pair<std::string, std::string>(name, value));
  }
  EXPECT_EQ(expected1, actual1);
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_TRUE(response_headers_modified1);

  // Check that we replace response headers only once.
  {
    EventResponseDelta d2("extid2", base::Time::FromInternalValue(1500));
    // Note that we use a different capitalization of KeY2. This should not
    // matter.
    d2.deleted_response_headers.push_back(
        ResponseHeader("KeY2", "Value2, Foo"));
    d2.added_response_headers.push_back(ResponseHeader("Key2", "Value4"));
    deltas.push_back(std::move(d2));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  bool response_headers_modified2;
  scoped_refptr<net::HttpResponseHeaders> new_headers2;
  GURL preserve_fragment_on_redirect_url2;
  MergeOnHeadersReceivedResponses(
      info, deltas, base_headers.get(), &new_headers2,
      &preserve_fragment_on_redirect_url2, &ignored_actions,
      &response_headers_modified2);
  ASSERT_TRUE(new_headers2.get());
  EXPECT_TRUE(preserve_fragment_on_redirect_url2.is_empty());
  iter = 0;
  std::multimap<std::string, std::string> actual2;
  while (new_headers2->EnumerateHeaderLines(&iter, &name, &value)) {
    actual2.insert(std::pair<std::string, std::string>(name, value));
  }
  EXPECT_EQ(expected1, actual2);
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid2",
                       web_request::IGNORED_ACTION_TYPE_RESPONSE_HEADERS));
  EXPECT_TRUE(response_headers_modified2);

  // Ensure headers removed by Declarative Net Request API can't be added by web
  // request extensions and result in a conflict.
  DNRRequestAction remove_headers_action =
      CreateRequestActionForTesting(DNRRequestAction::Type::REMOVE_HEADERS);
  remove_headers_action.response_headers_to_remove = {"key3"};
  info.dnr_actions = std::vector<DNRRequestAction>();
  info.dnr_actions->push_back(std::move(remove_headers_action));

  ignored_actions.clear();
  bool response_headers_modified3 = false;
  scoped_refptr<net::HttpResponseHeaders> new_headers3;
  GURL preserve_fragment_on_redirect_url3;
  MergeOnHeadersReceivedResponses(
      info, deltas, base_headers.get(), &new_headers3,
      &preserve_fragment_on_redirect_url3, &ignored_actions,
      &response_headers_modified3);
  ASSERT_TRUE(new_headers3.get());
  EXPECT_TRUE(preserve_fragment_on_redirect_url3.is_empty());
  iter = 0;
  std::multimap<std::string, std::string> actual3;
  while (new_headers3->EnumerateHeaderLines(&iter, &name, &value))
    actual3.emplace(name, value);
  std::multimap<std::string, std::string> expected3;
  expected3.emplace("Key2", "Value4");
  expected3.emplace("Key1", "Value1");
  EXPECT_EQ(expected3, actual3);
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid1",
                       web_request::IGNORED_ACTION_TYPE_RESPONSE_HEADERS));
  EXPECT_TRUE(response_headers_modified3);
}

// Check that we do not delete too much
TEST(ExtensionWebRequestHelpersTest,
     TestMergeOnHeadersReceivedResponsesDeletion) {
  helpers::IgnoredActions ignored_actions;
  std::string header_value;
  EventResponseDeltas deltas;

  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "Key1: Value2\r\n"
      "Key1: Value3\r\n"
      "Key2: Value4\r\n"
      "\r\n";
  auto base_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base_headers_string));

  {
    EventResponseDelta d1("extid1", base::Time::FromInternalValue(2000));
    d1.deleted_response_headers.push_back(ResponseHeader("KEY1", "Value2"));
    deltas.push_back(std::move(d1));
  }
  bool response_headers_modified1;
  scoped_refptr<net::HttpResponseHeaders> new_headers1;
  GURL preserve_fragment_on_redirect_url1;

  WebRequestInfoInitParams info_params;
  info_params.url = GURL(kExampleUrl);
  WebRequestInfo info(std::move(info_params));
  info.dnr_actions = std::vector<DNRRequestAction>();

  MergeOnHeadersReceivedResponses(
      info, deltas, base_headers.get(), &new_headers1,
      &preserve_fragment_on_redirect_url1, &ignored_actions,
      &response_headers_modified1);
  ASSERT_TRUE(new_headers1.get());
  EXPECT_TRUE(preserve_fragment_on_redirect_url1.is_empty());
  std::multimap<std::string, std::string> expected1;
  expected1.insert(std::pair<std::string, std::string>("Key1", "Value1"));
  expected1.insert(std::pair<std::string, std::string>("Key1", "Value3"));
  expected1.insert(std::pair<std::string, std::string>("Key2", "Value4"));
  size_t iter = 0;
  std::string name;
  std::string value;
  std::multimap<std::string, std::string> actual1;
  while (new_headers1->EnumerateHeaderLines(&iter, &name, &value)) {
    actual1.insert(std::pair<std::string, std::string>(name, value));
  }
  EXPECT_EQ(expected1, actual1);
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_TRUE(response_headers_modified1);
}

// Tests whether onHeadersReceived can initiate a redirect.
// The URL merge logic is shared with onBeforeRequest, so we only need to test
// whether the URLs are merged at all.
TEST(ExtensionWebRequestHelpersTest,
     TestMergeOnHeadersReceivedResponsesRedirect) {
  EventResponseDeltas deltas;
  helpers::IgnoredActions ignored_actions;

  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "\r\n";
  auto base_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base_headers_string));

  // No redirect
  {
    EventResponseDelta d0("extid0", base::Time::FromInternalValue(0));
    deltas.push_back(std::move(d0));
  }
  bool response_headers_modified0;
  scoped_refptr<net::HttpResponseHeaders> new_headers0;
  GURL preserve_fragment_on_redirect_url0;

  WebRequestInfoInitParams info_params;
  info_params.url = GURL(kExampleUrl);
  WebRequestInfo info(std::move(info_params));

  MergeOnHeadersReceivedResponses(
      info, deltas, base_headers.get(), &new_headers0,
      &preserve_fragment_on_redirect_url0, &ignored_actions,
      &response_headers_modified0);
  EXPECT_FALSE(new_headers0.get());
  EXPECT_TRUE(preserve_fragment_on_redirect_url0.is_empty());
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_FALSE(response_headers_modified0);

  // Single redirect.
  GURL new_url_1("http://foo.com");
  {
    EventResponseDelta d1("extid1", base::Time::FromInternalValue(1000));
    d1.new_url = GURL(new_url_1);
    deltas.push_back(std::move(d1));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  bool response_headers_modified1;

  scoped_refptr<net::HttpResponseHeaders> new_headers1;
  GURL preserve_fragment_on_redirect_url1;
  MergeOnHeadersReceivedResponses(
      info, deltas, base_headers.get(), &new_headers1,
      &preserve_fragment_on_redirect_url1, &ignored_actions,
      &response_headers_modified1);

  EXPECT_TRUE(new_headers1.get());
  EXPECT_TRUE(new_headers1->HasHeaderValue("Location", new_url_1.spec()));
  EXPECT_EQ(new_url_1, preserve_fragment_on_redirect_url1);
  EXPECT_TRUE(ignored_actions.empty());
  EXPECT_FALSE(response_headers_modified1);
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnAuthRequiredResponses) {
  helpers::IgnoredActions ignored_actions;
  EventResponseDeltas deltas;
  base::string16 username = base::ASCIIToUTF16("foo");
  base::string16 password = base::ASCIIToUTF16("bar");
  base::string16 password2 = base::ASCIIToUTF16("baz");

  // Check that we can handle if not returning credentials.
  {
    EventResponseDelta d0("extid0", base::Time::FromInternalValue(3000));
    deltas.push_back(std::move(d0));
  }
  net::AuthCredentials auth0;
  bool credentials_set =
      MergeOnAuthRequiredResponses(deltas, &auth0, &ignored_actions);
  EXPECT_FALSE(credentials_set);
  EXPECT_TRUE(auth0.Empty());
  EXPECT_EQ(0u, ignored_actions.size());

  // Check that we can set AuthCredentials.
  {
    EventResponseDelta d1("extid1", base::Time::FromInternalValue(2000));
    d1.auth_credentials = net::AuthCredentials(username, password);
    deltas.push_back(std::move(d1));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  net::AuthCredentials auth1;
  credentials_set =
      MergeOnAuthRequiredResponses(deltas, &auth1, &ignored_actions);
  EXPECT_TRUE(credentials_set);
  EXPECT_FALSE(auth1.Empty());
  EXPECT_EQ(username, auth1.username());
  EXPECT_EQ(password, auth1.password());
  EXPECT_EQ(0u, ignored_actions.size());

  // Check that we set AuthCredentials only once.
  {
    EventResponseDelta d2("extid2", base::Time::FromInternalValue(1500));
    d2.auth_credentials = net::AuthCredentials(username, password2);
    deltas.push_back(std::move(d2));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  net::AuthCredentials auth2;
  credentials_set =
      MergeOnAuthRequiredResponses(deltas, &auth2, &ignored_actions);
  EXPECT_TRUE(credentials_set);
  EXPECT_FALSE(auth2.Empty());
  EXPECT_EQ(username, auth1.username());
  EXPECT_EQ(password, auth1.password());
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid2",
                       web_request::IGNORED_ACTION_TYPE_AUTH_CREDENTIALS));

  // Check that we can set identical AuthCredentials twice without causing
  // a conflict.
  {
    EventResponseDelta d3("extid3", base::Time::FromInternalValue(1000));
    d3.auth_credentials = net::AuthCredentials(username, password);
    deltas.push_back(std::move(d3));
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  net::AuthCredentials auth3;
  credentials_set =
      MergeOnAuthRequiredResponses(deltas, &auth3, &ignored_actions);
  EXPECT_TRUE(credentials_set);
  EXPECT_FALSE(auth3.Empty());
  EXPECT_EQ(username, auth1.username());
  EXPECT_EQ(password, auth1.password());
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid2",
                       web_request::IGNORED_ACTION_TYPE_AUTH_CREDENTIALS));
}

}  // namespace extensions
