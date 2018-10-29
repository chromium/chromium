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
#include "chrome/browser/net/chrome_extensions_network_delegate.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/about_handler/about_protocol_handler.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/prefs/pref_member.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/api/web_request/upload_data_presenter.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/common/api/web_request.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/auth.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest-message.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/scoped_test_public_session_login_state.h"
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

namespace extensions {

namespace {

constexpr const char kExampleUrl[] = "http://example.com";

static void EventHandledOnIOThread(
    void* profile,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64_t request_id,
    ExtensionWebRequestEventRouter::EventResponse* response) {
  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      profile, extension_id, event_name, sub_event_name, request_id,
      response);
}

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

// Parses the JSON data attached to the |message| and tries to return it.
// |param| must outlive |out|.
void GetPartOfMessageArguments(IPC::Message* message,
                               const base::DictionaryValue** out,
                               ExtensionMsg_DispatchEvent::Param* param) {
  ASSERT_EQ(static_cast<uint32_t>(ExtensionMsg_DispatchEvent::ID),
            message->type());
  ASSERT_TRUE(ExtensionMsg_DispatchEvent::Read(message, param));
  const base::ListValue& list = std::get<1>(*param);
  ASSERT_EQ(1u, list.GetSize());
  ASSERT_TRUE(list.GetDictionary(0, out));
}

base::Value FormBinaryValue(base::StringPiece str) {
  base::Value list(base::Value::Type::LIST);
  list.GetList().emplace_back(base::Value(
      base::Value::BlobStorage(str.data(), str.data() + str.size())));
  return list;
}

base::Value FormStringValue(base::StringPiece str) {
  base::Value list(base::Value::Type::LIST);
  list.GetList().emplace_back(base::Value(str));
  return list;
}

// Returns a main-frame request to |url|.
std::unique_ptr<net::URLRequest> CreateRequestHelper(
    const GURL& url,
    net::TestURLRequestContext* context,
    net::TestDelegate* delegate) {
  CHECK(context);
  CHECK(delegate);

  std::unique_ptr<net::URLRequest> request = context->CreateRequest(
      url, net::DEFAULT_PRIORITY, delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  content::ResourceRequestInfo::AllocateForTesting(
      request.get(), content::RESOURCE_TYPE_MAIN_FRAME, /*context*/ nullptr,
      -1 /* render_process_id */, -1 /* render_view_id */,
      -1 /* render_frame_id */, true /* is_main_frame */,
      false /* allow_download */, false /* is_async */, content::PREVIEWS_OFF,
      ChromeNavigationUIData::CreateForMainFrameNavigation(
          nullptr /* web_contents */, WindowOpenDisposition::CURRENT_TAB));
  return request;
}

}  // namespace

// A mock event router that responds to events with a pre-arranged queue of
// Tasks.
class TestIPCSender : public IPC::Sender {
 public:
  typedef std::list<linked_ptr<IPC::Message> > SentMessages;

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

    sent_messages_.push_back(linked_ptr<IPC::Message>(message));
    return true;
  }

  base::queue<base::Closure> task_queue_;
  SentMessages sent_messages_;
};

class TestLogger : public WebRequestInfo::Logger {
 public:
  TestLogger() = default;
  ~TestLogger() override = default;

  size_t log_size() const { return events_.size(); }
  void clear() { events_.clear(); }

  // WebRequestInfo::Logger:
  void LogEvent(net::NetLogEventType event_type,
                const std::string& extension_id) override {
    events_.push_back({event_type, extension_id});
  }
  void LogBlockedBy(const std::string& blocker_info) override {}
  void LogUnblocked() override {}

 private:
  using Event = std::pair<net::NetLogEventType, std::string>;
  std::vector<Event> events_;

  DISALLOW_COPY_AND_ASSIGN(TestLogger);
};

class ExtensionWebRequestTest : public testing::Test {
 public:
  ExtensionWebRequestTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        profile_manager_(TestingBrowserProcess::GetGlobal()),
        event_router_(new EventRouterForwarder) {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    network_delegate_.reset(new ChromeNetworkDelegate(event_router_.get()));
    network_delegate_->set_profile(&profile_);
    network_delegate_->set_cookie_settings(
        CookieSettingsFactory::GetForProfile(&profile_).get());
    context_.reset(new net::TestURLRequestContext(true));
    context_->set_network_delegate(network_delegate_.get());
    context_->Init();
  }

  // Fires a URLRequest with the specified |method|, |content_type| and three
  // elements of upload data: bytes_1, a dummy empty file, bytes_2.
  void FireURLRequestWithData(const std::string& method,
                              const char* content_type,
                              const std::vector<char>& bytes_1,
                              const std::vector<char>& bytes_2);

  // Returns a main-frame request to |url|.
  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& url) {
    return CreateRequestHelper(url, context_.get(), &delegate_);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  TestingProfileManager profile_manager_;
  net::TestDelegate delegate_;
  TestIPCSender ipc_sender_;
  scoped_refptr<EventRouterForwarder> event_router_;
  std::unique_ptr<ChromeNetworkDelegate> network_delegate_;
  std::unique_ptr<net::TestURLRequestContext> context_;
};

// Tests that we handle disagreements among extensions about responses to
// blocking events (redirection) by choosing the response from the
// most-recently-installed extension.
TEST_F(ExtensionWebRequestTest, BlockingEventPrecedenceRedirect) {
  std::string extension1_id("1");
  std::string extension2_id("2");
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string kEventName(web_request::OnBeforeRequest::kEventName);
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension1_id, extension1_id, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, ExtraInfoSpec::BLOCKING, 0, 0,
      ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension2_id, extension2_id, events::FOR_TEST, kEventName,
      kEventName + "/2", filter, ExtraInfoSpec::BLOCKING, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  net::URLRequestJobFactoryImpl job_factory;
  job_factory.SetProtocolHandler(
      url::kAboutScheme,
      base::WrapUnique(new about_handler::AboutProtocolHandler()));
  context_->set_job_factory(&job_factory);

  GURL redirect_url("about:redirected");
  GURL not_chosen_redirect_url("about:not_chosen");

  std::unique_ptr<net::URLRequest> request = CreateRequest(GURL("about:blank"));
  {
    // onBeforeRequest will be dispatched twice initially. The second response -
    // the redirect - should win, since it has a later |install_time|. The
    // redirect will dispatch another pair of onBeforeRequest. There, the first
    // response should win (later |install_time|).
    ExtensionWebRequestEventRouter::EventResponse* response = NULL;

    // Extension1 response. Arrives first, but ignored due to install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    response->new_url = not_chosen_redirect_url;
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request->identifier(), response));

    // Extension2 response. Arrives second, and chosen because of install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    response->new_url = redirect_url;
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request->identifier(), response));

    // Extension2 response to the redirected URL. Arrives first, and chosen.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request->identifier(), response));

    // Extension1 response to the redirected URL. Arrives second, and ignored.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request->identifier(), response));

    request->Start();
    base::RunLoop().Run();

    EXPECT_TRUE(!request->is_pending());
    EXPECT_EQ(net::OK, delegate_.request_status());
    EXPECT_EQ(redirect_url, request->url());
    EXPECT_EQ(2U, request->url_chain().size());
    EXPECT_EQ(0U, ipc_sender_.GetNumTasks());
  }

  // Now test the same thing but the extensions answer in reverse order.
  std::unique_ptr<net::URLRequest> request2 =
      CreateRequest(GURL("about:blank"));
  {
    ExtensionWebRequestEventRouter::EventResponse* response = NULL;

    // Extension2 response. Arrives first, and chosen because of install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    response->new_url = redirect_url;
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request2->identifier(), response));

    // Extension1 response. Arrives second, but ignored due to install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    response->new_url = not_chosen_redirect_url;
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request2->identifier(), response));

    // Extension2 response to the redirected URL. Arrives first, and chosen.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request2->identifier(), response));

    // Extension1 response to the redirected URL. Arrives second, and ignored.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request2->identifier(), response));

    request2->Start();
    base::RunLoop().Run();

    EXPECT_TRUE(!request2->is_pending());
    EXPECT_EQ(net::OK, delegate_.request_status());
    EXPECT_EQ(redirect_url, request2->url());
    EXPECT_EQ(2U, request2->url_chain().size());
    EXPECT_EQ(0U, ipc_sender_.GetNumTasks());
  }

  ExtensionWebRequestEventRouter::EventListener::ID id1(
      &profile_, extension1_id, kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::EventListener::ID id2(
      &profile_, extension2_id, kEventName + "/2", 0, 0);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id2,
                                                                     false);
}

// Test that a request is canceled if this is requested by any extension
// regardless whether it is the extension with the highest precedence.
TEST_F(ExtensionWebRequestTest, BlockingEventPrecedenceCancel) {
  std::string extension1_id("1");
  std::string extension2_id("2");
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string kEventName(web_request::OnBeforeRequest::kEventName);
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension1_id, extension1_id, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, ExtraInfoSpec::BLOCKING, 0, 0,
      ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension2_id, extension2_id, events::FOR_TEST, kEventName,
      kEventName + "/2", filter, ExtraInfoSpec::BLOCKING, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  GURL request_url("about:blank");
  std::unique_ptr<net::URLRequest> request = CreateRequest(request_url);

  // onBeforeRequest will be dispatched twice. The second response -
  // the redirect - would win, since it has a later |install_time|, but
  // the first response takes precedence because cancel >> redirect.
  GURL redirect_url("about:redirected");
  ExtensionWebRequestEventRouter::EventResponse* response = NULL;

  // Extension1 response. Arrives first, would be ignored in principle due to
  // install_time but "cancel" always wins.
  response = new ExtensionWebRequestEventRouter::EventResponse(
      extension1_id, base::Time::FromDoubleT(1));
  response->cancel = true;
  ipc_sender_.PushTask(
      base::Bind(&EventHandledOnIOThread,
          &profile_, extension1_id, kEventName, kEventName + "/1",
          request->identifier(), response));

  // Extension2 response. Arrives second, but has higher precedence
  // due to its later install_time.
  response = new ExtensionWebRequestEventRouter::EventResponse(
      extension2_id, base::Time::FromDoubleT(2));
  response->new_url = redirect_url;
  ipc_sender_.PushTask(
      base::Bind(&EventHandledOnIOThread,
          &profile_, extension2_id, kEventName, kEventName + "/2",
          request->identifier(), response));

  request->Start();

  base::RunLoop().Run();

  EXPECT_TRUE(!request->is_pending());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, delegate_.request_status());
  EXPECT_EQ(request_url, request->url());
  EXPECT_EQ(1U, request->url_chain().size());
  EXPECT_EQ(0U, ipc_sender_.GetNumTasks());

  ExtensionWebRequestEventRouter::EventListener::ID id1(
      &profile_, extension1_id, kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::EventListener::ID id2(
      &profile_, extension2_id, kEventName + "/2", 0, 0);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id2,
                                                                     false);
}

TEST_F(ExtensionWebRequestTest, SimulateChancelWhileBlocked) {
  // We subscribe to OnBeforeRequest and OnErrorOccurred.
  // While the OnBeforeRequest handler is blocked, we cancel the request.
  // We verify that the response of the blocked OnBeforeRequest handler
  // is ignored.

  std::string extension_id("1");
  ExtensionWebRequestEventRouter::RequestFilter filter;

  // Subscribe to OnBeforeRequest and OnErrorOccurred.
  const std::string kEventName(web_request::OnBeforeRequest::kEventName);
  const std::string kEventName2(web_request::OnErrorOccurred::kEventName);
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id, extension_id, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, ExtraInfoSpec::BLOCKING, 0, 0,
      ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id, extension_id, events::FOR_TEST, kEventName2,
      kEventName2 + "/1", filter, 0, 0, 0, ipc_sender_factory.GetWeakPtr());

  GURL request_url("about:blank");
  std::unique_ptr<net::URLRequest> request = CreateRequest(request_url);

  ExtensionWebRequestEventRouter::EventResponse* response = NULL;

  // Extension response for the OnBeforeRequest handler. This should not be
  // processed because request is canceled before the handler responds.
  response = new ExtensionWebRequestEventRouter::EventResponse(
      extension_id, base::Time::FromDoubleT(1));
  GURL redirect_url("about:redirected");
  response->new_url = redirect_url;
  ipc_sender_.PushTask(
      base::Bind(&EventHandledOnIOThread,
          &profile_, extension_id, kEventName, kEventName + "/1",
          request->identifier(), response));

  base::RunLoop run_loop;

  // Extension response for OnErrorOccurred: Terminate the message loop.
  ipc_sender_.PushTask(
      base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                 base::ThreadTaskRunnerHandle::Get(), FROM_HERE,
                 run_loop.QuitWhenIdleClosure()));

  request->Start();
  // request->Start() will have submitted OnBeforeRequest by the time we cancel.
  int net_error = request->Cancel();
  run_loop.Run();

  EXPECT_EQ(net::ERR_ABORTED, net_error);
  EXPECT_TRUE(!request->is_pending());
  EXPECT_EQ(request_url, request->url());
  EXPECT_EQ(1U, request->url_chain().size());
  EXPECT_EQ(0U, ipc_sender_.GetNumTasks());

  ExtensionWebRequestEventRouter::EventListener::ID id1(
      &profile_, extension_id, kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::EventListener::ID id2(
      &profile_, extension_id, kEventName2 + "/1", 0, 0);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id2,
                                                                     false);
}

namespace {

// Create the numerical representation of |values|, strings passed as
// extraInfoSpec by the event handler. Returns true on success, otherwise false.
bool GenerateInfoSpec(const std::string& values, int* result) {
  // Create a base::ListValue of strings.
  base::ListValue list_value;
  for (const std::string& cur :
       base::SplitString(values, ",", base::KEEP_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY))
    list_value.AppendString(cur);
  return ExtraInfoSpec::InitFromValue(list_value, result);
}

}  // namespace

void ExtensionWebRequestTest::FireURLRequestWithData(
    const std::string& method,
    const char* content_type,
    const std::vector<char>& bytes_1,
    const std::vector<char>& bytes_2) {
  // The request URL can be arbitrary but must have an HTTP or HTTPS scheme.
  GURL request_url("http://www.example.com");
  std::unique_ptr<net::URLRequest> request = CreateRequest(request_url);
  request->set_method(method);
  if (content_type != NULL) {
    request->SetExtraRequestHeaderByName(net::HttpRequestHeaders::kContentType,
                                         content_type,
                                         true /* overwrite */);
  }
  std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<net::UploadBytesElementReader>(
      &(bytes_1[0]), bytes_1.size()));
  element_readers.push_back(std::make_unique<net::UploadFileElementReader>(
      base::ThreadTaskRunnerHandle::Get().get(), base::FilePath(), 0, 0,
      base::Time()));
  element_readers.push_back(std::make_unique<net::UploadBytesElementReader>(
      &(bytes_2[0]), bytes_2.size()));
  request->set_upload(std::make_unique<net::ElementsUploadDataStream>(
      std::move(element_readers), 0));
  ipc_sender_.PushTask(base::DoNothing());
  request->Start();
}

TEST_F(ExtensionWebRequestTest, AccessRequestBodyData) {
  // We verify that URLRequest body is accessible to OnBeforeRequest listeners.
  // These testing steps are repeated twice in a row:
  // 1. Register an extension requesting "requestBody" in ExtraInfoSpec and
  //    file a POST URLRequest with a multipart-encoded form. See it getting
  //    parsed.
  // 2. Do the same, but without requesting "requestBody". Nothing should be
  //    parsed.
  // 3. With "requestBody", fire a POST URLRequest which is not a parseable
  //    HTML form. Raw data should be returned.
  // 4. Do the same, but with a PUT method. Result should be the same.
  const std::string kMethodPost("POST");
  const std::string kMethodPut("PUT");

  // Input.
  const char kPlainBlock1[] = "abcd\n";
  const size_t kPlainBlock1Length = sizeof(kPlainBlock1) - 1;
  std::vector<char> plain_1(kPlainBlock1, kPlainBlock1 + kPlainBlock1Length);
  const char kPlainBlock2[] = "1234\n";
  const size_t kPlainBlock2Length = sizeof(kPlainBlock2) - 1;
  std::vector<char> plain_2(kPlainBlock2, kPlainBlock2 + kPlainBlock2Length);
#define kBoundary "THIS_IS_A_BOUNDARY"
  const char kFormBlock1[] =
      "--" kBoundary
      "\r\n"
      "Content-Disposition: form-data; name=\"A\"\r\n"
      "\r\n"
      "test text\r\n"
      "--" kBoundary
      "\r\n"
      "Content-Disposition: form-data; name=\"B\"; filename=\"\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "--" kBoundary
      "\r\n"
      "Content-Disposition: form-data; name=\"B_content\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "\uffff\uffff\uffff\uffff\r\n"
      "--" kBoundary "\r\n";
  std::vector<char> form_1(kFormBlock1, kFormBlock1 + sizeof(kFormBlock1) - 1);
  const char kFormBlock2[] =
      "Content-Disposition: form-data; name=\"C\"\r\n"
      "\r\n"
      "test password\r\n"
      "--" kBoundary "--";
  std::vector<char> form_2(kFormBlock2, kFormBlock2 + sizeof(kFormBlock2) - 1);

  // Expected output.
  // Paths to look for in returned dictionaries.
  const std::string kBodyPath(keys::kRequestBodyKey);
  const std::string kFormDataPath(
      kBodyPath + "." + keys::kRequestBodyFormDataKey);
  const std::string kRawPath(kBodyPath + "." + keys::kRequestBodyRawKey);
  const std::string kErrorPath(kBodyPath + "." + keys::kRequestBodyErrorKey);
  const std::string* const kPath[] = {
    &kFormDataPath,
    &kBodyPath,
    &kRawPath,
    &kRawPath
  };
  // Contents of formData.
  struct KeyValuePairs {
    const char* key;
    base::Value value;
  };
  KeyValuePairs kFormDataPairs[] = {
      {"A", FormStringValue("test text")},
      {"B", FormStringValue("")},
      {"B_content", FormBinaryValue("\uffff\uffff\uffff\uffff")},
      {"C", FormStringValue("test password")}};
  std::unique_ptr<base::Value> form_data =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  for (auto& pair : kFormDataPairs) {
    form_data->SetKey(pair.key, std::move(pair.value));
  }

  ASSERT_TRUE(form_data.get() != NULL);
  ASSERT_TRUE(form_data->type() == base::Value::Type::DICTIONARY);
  // Contents of raw.
  base::ListValue raw;
  extensions::subtle::AppendKeyValuePair(
      keys::kRequestBodyRawBytesKey,
      Value::CreateWithCopiedBuffer(kPlainBlock1, kPlainBlock1Length), &raw);
  extensions::subtle::AppendKeyValuePair(
      keys::kRequestBodyRawFileKey,
      std::make_unique<base::Value>(std::string()), &raw);
  extensions::subtle::AppendKeyValuePair(
      keys::kRequestBodyRawBytesKey,
      Value::CreateWithCopiedBuffer(kPlainBlock2, kPlainBlock2Length), &raw);
  // Summary.
  const base::Value* const kExpected[] = {
    form_data.get(),
    NULL,
    &raw,
    &raw,
  };
  static_assert(arraysize(kPath) == arraysize(kExpected),
                "kPath and kExpected arrays should have the same number "
                "of elements");
  // Header.
  const char kMultipart[] = "multipart/form-data; boundary=" kBoundary;
#undef kBoundary

  // Set up a dummy extension name.
  const std::string kEventName(web_request::OnBeforeRequest::kEventName);
  ExtensionWebRequestEventRouter::RequestFilter filter;
  std::string extension_id("1");
  const std::string string_spec_post("blocking,requestBody");
  const std::string string_spec_no_post("blocking");
  int extra_info_spec_empty = 0;
  int extra_info_spec_body = 0;
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);

  // Part 1.
  // Subscribe to OnBeforeRequest with requestBody requirement.
  ASSERT_TRUE(GenerateInfoSpec(string_spec_post, &extra_info_spec_body));
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id, extension_id, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, extra_info_spec_body, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  FireURLRequestWithData(kMethodPost, kMultipart, form_1, form_2);

  // We inspect the result in the message list of |ipc_sender_| later.
  base::RunLoop().RunUntilIdle();

  ExtensionWebRequestEventRouter::EventListener::ID id1(
      &profile_, extension_id, kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);

  // Part 2.
  // Now subscribe to OnBeforeRequest *without* the requestBody requirement.
  ASSERT_TRUE(
      GenerateInfoSpec(string_spec_no_post, &extra_info_spec_empty));
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id, extension_id, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, extra_info_spec_empty, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  FireURLRequestWithData(kMethodPost, kMultipart, form_1, form_2);

  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);

  // Subscribe to OnBeforeRequest with requestBody requirement.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id, extension_id, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, extra_info_spec_body, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  // Part 3.
  // Now send a POST request with body which is not parseable as a form.
  FireURLRequestWithData(kMethodPost, NULL /*no header*/, plain_1, plain_2);

  // Part 4.
  // Now send a PUT request with the same body as above.
  FireURLRequestWithData(kMethodPut, NULL /*no header*/, plain_1, plain_2);

  base::RunLoop().RunUntilIdle();

  // Clean-up.
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);

  IPC::Message* message = NULL;
  auto i = ipc_sender_.sent_begin();
  for (size_t test = 0; test < arraysize(kExpected); ++test) {
    SCOPED_TRACE(testing::Message("iteration number ") << test);
    EXPECT_NE(i, ipc_sender_.sent_end());
    message = (i++)->get();
    const base::DictionaryValue* details = nullptr;
    ExtensionMsg_DispatchEvent::Param param;
    GetPartOfMessageArguments(message, &details, &param);
    ASSERT_TRUE(details != NULL);
    const base::Value* result = NULL;
    if (kExpected[test]) {
      EXPECT_TRUE(details->Get(*(kPath[test]), &result));
      EXPECT_TRUE(kExpected[test]->Equals(result));
    } else {
      EXPECT_FALSE(details->Get(*(kPath[test]), &result));
    }
  }

  EXPECT_EQ(i, ipc_sender_.sent_end());
}

// Tests whether requestBody is only present on the events that requested it.
TEST_F(ExtensionWebRequestTest, MinimalAccessRequestBodyData) {
  const std::string kEventName(web_request::OnBeforeRequest::kEventName);
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string extension_id1("1");
  const std::string extension_id2("2");
  int extra_info_spec_body = 0;
  int extra_info_spec_empty = 0;
  ASSERT_TRUE(GenerateInfoSpec("requestBody", &extra_info_spec_body));
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);

  bool kExpected[] = {
    true,
    false,
    false,
    true,
  };

  // Extension 1 with requestBody spec.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id1, extension_id1, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, extra_info_spec_body, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  // Extension 1 without requestBody spec.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id1, extension_id1, events::FOR_TEST, kEventName,
      kEventName + "/2", filter, extra_info_spec_empty, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  // Extension 2, without requestBody spec.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id2, extension_id2, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, extra_info_spec_empty, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  // Extension 2, with requestBody spec.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id2, extension_id2, events::FOR_TEST, kEventName,
      kEventName + "/2", filter, extra_info_spec_body, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  // Only one request is sent, but more than one event will be triggered.
  for (size_t i = 1; i < arraysize(kExpected); ++i)
    ipc_sender_.PushTask(base::DoNothing());

  const std::vector<char> part_of_body(1);
  FireURLRequestWithData("POST", nullptr, part_of_body, part_of_body);

  base::RunLoop().RunUntilIdle();

  // Clean-up
  ExtensionWebRequestEventRouter::EventListener::ID id1(
      &profile_, extension_id1, kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::EventListener::ID id2(
      &profile_, extension_id1, kEventName + "/2", 0, 0);
  ExtensionWebRequestEventRouter::EventListener::ID id3(
      &profile_, extension_id2, kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::EventListener::ID id4(
      &profile_, extension_id2, kEventName + "/2", 0, 0);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id2,
                                                                     false);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id3,
                                                                     false);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id4,
                                                                     false);

  auto i = ipc_sender_.sent_begin();

  for (size_t test = 0; test < arraysize(kExpected); ++test, ++i) {
    SCOPED_TRACE(testing::Message("iteration number ") << test);
    EXPECT_NE(i, ipc_sender_.sent_end());
    IPC::Message* message = i->get();
    const base::DictionaryValue* details = nullptr;
    ExtensionMsg_DispatchEvent::Param param;
    GetPartOfMessageArguments(message, &details, &param);
    ASSERT_TRUE(details != nullptr);
    EXPECT_EQ(kExpected[test], details->HasKey(keys::kRequestBodyKey));
  }

  EXPECT_EQ(i, ipc_sender_.sent_end());
}

#if defined(OS_CHROMEOS)
// Tests that proper filtering is applied in public session (non-whitelisted
// extension gets some things filtered out, while there's no filtering applied
// for a whitelisted extension).
TEST_F(ExtensionWebRequestTest, ProperFilteringInPublicSession) {
  chromeos::ScopedTestPublicSessionLoginState state;
  const std::string kEventName(web_request::OnBeforeRequest::kEventName);
  ExtensionWebRequestEventRouter::RequestFilter filter;
  // Whitelisted extension (User Agent Switcher).
  const std::string extension_id1("djflhoibgkdhkhhcedjiklpkjnoahfmg");
  const std::string extension_id2 =
      crx_file::id_util::GenerateId("nonwhitelisted");
  int extra_info_spec_body = 0;
  ASSERT_TRUE(GenerateInfoSpec("requestBody", &extra_info_spec_body));
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);

  bool kExpected[] = {
    true,
    false,
  };

  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id1, extension_id1, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, extra_info_spec_body, 0, 0,
      ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id2, extension_id2, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, extra_info_spec_body, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  // Only one request is sent, but more than one event will be triggered.
  for (size_t i = 1; i < arraysize(kExpected); ++i)
    ipc_sender_.PushTask(base::DoNothing());

  const std::vector<char> part_of_body(1);
  FireURLRequestWithData("POST", nullptr, part_of_body, part_of_body);

  base::RunLoop().RunUntilIdle();

  // Clean-up
  ExtensionWebRequestEventRouter::EventListener::ID id1(
      &profile_, extension_id1, kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::EventListener::ID id2(
      &profile_, extension_id2, kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id2,
                                                                     false);

  TestIPCSender::SentMessages::const_iterator i = ipc_sender_.sent_begin();

  for (size_t test = 0; test < arraysize(kExpected); ++test, ++i) {
    SCOPED_TRACE(testing::Message("iteration number ") << test);
    EXPECT_NE(i, ipc_sender_.sent_end());
    IPC::Message* message = i->get();
    const base::DictionaryValue* details = nullptr;
    ExtensionMsg_DispatchEvent::Param param;
    GetPartOfMessageArguments(message, &details, &param);
    ASSERT_TRUE(details != nullptr);
    EXPECT_EQ(kExpected[test], details->HasKey(keys::kRequestBodyKey));
  }

  EXPECT_EQ(i, ipc_sender_.sent_end());
}
#endif

TEST_F(ExtensionWebRequestTest, NoAccessRequestBodyData) {
  // We verify that URLRequest body is NOT accessible to OnBeforeRequest
  // listeners when the type of the request is different from POST or PUT, or
  // when the request body is empty. 3 requests are fired, without upload data,
  // a POST, PUT and GET request. For none of them the "requestBody" object
  // property should be present in the details passed to the onBeforeRequest
  // event listener.
  const char* const kMethods[] = { "POST", "PUT", "GET" };

  // Set up a dummy extension name.
  const std::string kEventName(web_request::OnBeforeRequest::kEventName);
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string extension_id("1");
  int extra_info_spec = 0;
  ASSERT_TRUE(GenerateInfoSpec("blocking,requestBody", &extra_info_spec));
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);

  // Subscribe to OnBeforeRequest with requestBody requirement.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id, extension_id, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, extra_info_spec, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  // The request URL can be arbitrary but must have an HTTP or HTTPS scheme.
  const GURL request_url("http://www.example.com");

  for (size_t i = 0; i < arraysize(kMethods); ++i) {
    std::unique_ptr<net::URLRequest> request = CreateRequest(request_url);
    request->set_method(kMethods[i]);
    ipc_sender_.PushTask(base::DoNothing());
    request->Start();
  }

  // We inspect the result in the message list of |ipc_sender_| later.
  base::RunLoop().RunUntilIdle();

  ExtensionWebRequestEventRouter::EventListener::ID id1(
      &profile_, extension_id, kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);

  auto i = ipc_sender_.sent_begin();
  for (size_t test = 0; test < arraysize(kMethods); ++test, ++i) {
    SCOPED_TRACE(testing::Message("iteration number ") << test);
    EXPECT_NE(i, ipc_sender_.sent_end());
    IPC::Message* message = i->get();
    const base::DictionaryValue* details = nullptr;
    ExtensionMsg_DispatchEvent::Param param;
    GetPartOfMessageArguments(message, &details, &param);
    ASSERT_TRUE(details != NULL);
    EXPECT_FALSE(details->HasKey(keys::kRequestBodyKey));
  }

  EXPECT_EQ(i, ipc_sender_.sent_end());
}

// Tests that |embedder_process_id| is not relevant for adding and removing
// listeners with |web_view_instance_id| = 0.
TEST_F(ExtensionWebRequestTest, AddAndRemoveListeners) {
  std::string ext_id("abcdefghijklmnopabcdefghijklmnop");
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string kEventName(web_request::OnBeforeRequest::kEventName);
  const std::string kSubEventName = kEventName + "/1";
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);
  EXPECT_EQ(
      0u,
      ExtensionWebRequestEventRouter::GetInstance()->GetListenerCountForTesting(
          &profile_, kEventName));

  // Add two non-webview listeners.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, ext_id, ext_id, events::FOR_TEST, kEventName, kSubEventName,
      filter, 0, 1 /* embedder_process_id */, 0,
      ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, ext_id, ext_id, events::FOR_TEST, kEventName, kSubEventName,
      filter, 0, 2 /* embedder_process_id */, 0,
      ipc_sender_factory.GetWeakPtr());
  EXPECT_EQ(
      2u,
      ExtensionWebRequestEventRouter::GetInstance()->GetListenerCountForTesting(
          &profile_, kEventName));

  // Now remove the events without passing an explicit process ID.
  ExtensionWebRequestEventRouter::EventListener::ID id1(&profile_, ext_id,
                                                        kSubEventName, 0, 0);
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

// The set of blocked requests should not grow unbounded.
TEST_F(ExtensionWebRequestTest, BlockedRequestsAreRemoved) {
  std::string extension_id("1");
  ExtensionWebRequestEventRouter::RequestFilter filter;

  // Subscribe to OnBeforeRequest.
  const std::string kEventName(web_request::OnBeforeRequest::kEventName);
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension_id, extension_id, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, ExtraInfoSpec::BLOCKING, 0, 0,
      ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::EventListener::ID id(&profile_, extension_id,
                                                       kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::EventListener* listener =
      ExtensionWebRequestEventRouter::GetInstance()->FindEventListener(id);
  ASSERT_NE(nullptr, listener);
  EXPECT_EQ(0u, listener->blocked_requests.size());

  // Send a request. It should block. Wait for the run loop to become idle.
  GURL request_url("about:blank");
  std::unique_ptr<net::URLRequest> request = CreateRequest(request_url);
  // Extension response for OnErrorOccurred: Terminate the message loop.
  {
    base::RunLoop run_loop;
    ipc_sender_.PushTask(
        base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                   base::ThreadTaskRunnerHandle::Get(), FROM_HERE,
                   run_loop.QuitWhenIdleClosure()));
    request->Start();
    run_loop.Run();
  }

  // Confirm that there is a blocked request.
  EXPECT_EQ(1u, listener->blocked_requests.size());

  // Send a response through.
  ExtensionWebRequestEventRouter::EventResponse* response =
      new ExtensionWebRequestEventRouter::EventResponse(
          extension_id, base::Time::FromDoubleT(1));
  response->cancel = true;
  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      &profile_, extension_id, kEventName, kEventName + "/1",
      request->identifier(), response);
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Now there should be no blocked requests.
  EXPECT_EQ(0u, listener->blocked_requests.size());

  EXPECT_TRUE(!request->is_pending());
  EXPECT_EQ(net::URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, request->status().error());
  EXPECT_EQ(0U, ipc_sender_.GetNumTasks());

  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id, false);
}

struct HeaderModificationTest_Header {
  const char* name;
  const char* value;
};

struct HeaderModificationTest_Modification {
  enum Type {
    SET,
    REMOVE
  };

  int extension_id;
  Type type;
  const char* key;
  const char* value;
};

struct HeaderModificationTest {
  int before_size;
  HeaderModificationTest_Header before[10];
  int modification_size;
  HeaderModificationTest_Modification modification[10];
  int after_size;
  HeaderModificationTest_Header after[10];
};

class ExtensionWebRequestHeaderModificationTest
    : public testing::TestWithParam<HeaderModificationTest> {
 public:
  ExtensionWebRequestHeaderModificationTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        profile_manager_(TestingBrowserProcess::GetGlobal()),
        event_router_(new EventRouterForwarder) {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    network_delegate_.reset(new ChromeNetworkDelegate(event_router_.get()));
    network_delegate_->set_profile(&profile_);
    network_delegate_->set_cookie_settings(
        CookieSettingsFactory::GetForProfile(&profile_).get());
    context_.reset(new net::TestURLRequestContext(true));
    host_resolver_.reset(new net::MockHostResolver());
    host_resolver_->rules()->AddSimulatedFailure("doesnotexist");
    context_->set_host_resolver(host_resolver_.get());
    context_->set_network_delegate(network_delegate_.get());
    context_->Init();
  }

  // Returns a main-frame request to |url|.
  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& url) {
    return CreateRequestHelper(url, context_.get(), &delegate_);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  TestingProfileManager profile_manager_;
  net::TestDelegate delegate_;
  TestIPCSender ipc_sender_;
  scoped_refptr<EventRouterForwarder> event_router_;
  std::unique_ptr<ChromeNetworkDelegate> network_delegate_;
  std::unique_ptr<net::MockHostResolver> host_resolver_;
  std::unique_ptr<net::TestURLRequestContext> context_;
};

TEST_P(ExtensionWebRequestHeaderModificationTest, TestModifications) {
  std::string extension1_id("1");
  std::string extension2_id("2");
  std::string extension3_id("3");
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string kEventName(keys::kOnBeforeSendHeadersEvent);
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);

  // Install two extensions that can modify headers. Extension 2 has
  // higher precedence than extension 1.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension1_id, extension1_id, events::FOR_TEST, kEventName,
      kEventName + "/1", filter, ExtraInfoSpec::BLOCKING, 0, 0,
      ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension2_id, extension2_id, events::FOR_TEST, kEventName,
      kEventName + "/2", filter, ExtraInfoSpec::BLOCKING, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  // Install one extension that observes the final headers.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension3_id, extension3_id, events::FOR_TEST,
      keys::kOnSendHeadersEvent, std::string(keys::kOnSendHeadersEvent) + "/3",
      filter, ExtraInfoSpec::REQUEST_HEADERS, 0, 0,
      ipc_sender_factory.GetWeakPtr());

  GURL request_url("http://doesnotexist/does_not_exist.html");
  std::unique_ptr<net::URLRequest> request = CreateRequest(request_url);

  // Initialize headers available before extensions are notified of the
  // onBeforeSendHeaders event.
  HeaderModificationTest test = GetParam();
  net::HttpRequestHeaders before_headers;
  for (int i = 0; i < test.before_size; ++i)
    before_headers.SetHeader(test.before[i].name, test.before[i].value);
  request->SetExtraRequestHeaders(before_headers);

  // Gather the modifications to the headers for the respective extensions.
  // We assume here that all modifications of one extension are listed
  // in a continuous block of |test.modifications_|.
  ExtensionWebRequestEventRouter::EventResponse* response = NULL;
  for (int i = 0; i < test.modification_size; ++i) {
    const HeaderModificationTest_Modification& mod = test.modification[i];
    if (response == NULL) {
      response = new ExtensionWebRequestEventRouter::EventResponse(
          mod.extension_id == 1 ? extension1_id : extension2_id,
          base::Time::FromDoubleT(mod.extension_id));
      response->request_headers.reset(new net::HttpRequestHeaders());
      response->request_headers->MergeFrom(request->extra_request_headers());
    }

    switch (mod.type) {
      case HeaderModificationTest_Modification::SET:
        response->request_headers->SetHeader(mod.key, mod.value);
        break;
      case HeaderModificationTest_Modification::REMOVE:
        response->request_headers->RemoveHeader(mod.key);
        break;
    }

    // Trigger the result when this is the last modification statement or
    // the block of modifications for the next extension starts.
    if (i+1 == test.modification_size ||
        mod.extension_id != test.modification[i+1].extension_id) {
      ipc_sender_.PushTask(
          base::Bind(&EventHandledOnIOThread,
              &profile_, mod.extension_id == 1 ? extension1_id : extension2_id,
              kEventName, kEventName + (mod.extension_id == 1 ? "/1" : "/2"),
              request->identifier(), response));
      response = NULL;
    }
  }

  // Don't do anything for the onSendHeaders message.
  ipc_sender_.PushTask(base::DoNothing());

  // Note that we mess up the headers slightly:
  // request->Start() will first add additional headers (e.g. the User-Agent)
  // and then send an event to the extension. When we have prepared our
  // answers to the onBeforeSendHeaders events above, these headers did not
  // exists and are therefore not listed in the responses. This makes
  // them seem deleted.
  request->Start();
  base::RunLoop().Run();

  EXPECT_TRUE(!request->is_pending());
  // This cannot succeed as we send the request to a server that does not exist.
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, delegate_.request_status());
  EXPECT_EQ(request_url, request->url());
  EXPECT_EQ(1U, request->url_chain().size());
  EXPECT_EQ(0U, ipc_sender_.GetNumTasks());

  // Calculate the expected headers.
  net::HttpRequestHeaders expected_headers;
  for (int i = 0; i < test.after_size; ++i) {
    expected_headers.SetHeader(test.after[i].name,
                               test.after[i].value);
  }

  // Counter for the number of observed onSendHeaders events.
  int num_headers_observed = 0;

  // Search the onSendHeaders signal in the IPC messages and check that
  // it contained the correct headers.
  TestIPCSender::SentMessages::const_iterator i;
  for (i = ipc_sender_.sent_begin(); i != ipc_sender_.sent_end(); ++i) {
    IPC::Message* message = i->get();
    if (ExtensionMsg_DispatchEvent::ID != message->type())
      continue;
    ExtensionMsg_DispatchEvent::Param message_tuple;
    ExtensionMsg_DispatchEvent::Read(message, &message_tuple);
    const ExtensionMsg_DispatchEvent_Params& params =
        std::get<0>(message_tuple);

    if (params.event_name != std::string(keys::kOnSendHeadersEvent) + "/3")
      continue;

    const base::ListValue& event_args = std::get<1>(message_tuple);
    const base::DictionaryValue* event_arg_dict = nullptr;
    ASSERT_TRUE(event_args.GetDictionary(0, &event_arg_dict));

    const base::ListValue* request_headers = nullptr;
    ASSERT_TRUE(event_arg_dict->GetList(keys::kRequestHeadersKey,
                                        &request_headers));

    net::HttpRequestHeaders observed_headers;
    for (size_t j = 0; j < request_headers->GetSize(); ++j) {
      const base::DictionaryValue* header = nullptr;
      ASSERT_TRUE(request_headers->GetDictionary(j, &header));
      std::string key;
      std::string value;
      ASSERT_TRUE(header->GetString(keys::kHeaderNameKey, &key));
      ASSERT_TRUE(header->GetString(keys::kHeaderValueKey, &value));
      observed_headers.SetHeader(key, value);
    }

    EXPECT_EQ(expected_headers.ToString(), observed_headers.ToString());
    ++num_headers_observed;
  }
  EXPECT_EQ(1, num_headers_observed);
  ExtensionWebRequestEventRouter::EventListener::ID id1(
      &profile_, extension1_id, kEventName + "/1", 0, 0);
  ExtensionWebRequestEventRouter::EventListener::ID id2(
      &profile_, extension2_id, kEventName + "/2", 0, 0);
  ExtensionWebRequestEventRouter::EventListener::ID id3(
      &profile_, extension3_id, std::string(keys::kOnSendHeadersEvent) + "/3",
      0, 0);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id1,
                                                                     false);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id2,
                                                                     false);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(id3,
                                                                     false);
};

namespace {

void TestInitFromValue(const std::string& values, bool expected_return_code,
                       int expected_extra_info_spec) {
  int actual_info_spec;
  bool actual_return_code = GenerateInfoSpec(values, &actual_info_spec);
  EXPECT_EQ(expected_return_code, actual_return_code);
  if (expected_return_code)
    EXPECT_EQ(expected_extra_info_spec, actual_info_spec);
}

}  // namespace

TEST_F(ExtensionWebRequestTest, InitFromValue) {
  TestInitFromValue(std::string(), true, 0);

  // Single valid values.
  TestInitFromValue(
      "requestHeaders",
      true,
      ExtraInfoSpec::REQUEST_HEADERS);
  TestInitFromValue(
      "responseHeaders",
      true,
      ExtraInfoSpec::RESPONSE_HEADERS);
  TestInitFromValue(
      "blocking",
      true,
      ExtraInfoSpec::BLOCKING);
  TestInitFromValue(
      "asyncBlocking",
      true,
      ExtraInfoSpec::ASYNC_BLOCKING);
  TestInitFromValue(
      "requestBody",
      true,
      ExtraInfoSpec::REQUEST_BODY);

  // Multiple valid values are bitwise-or'ed.
  TestInitFromValue(
      "requestHeaders,blocking",
      true,
      ExtraInfoSpec::REQUEST_HEADERS | ExtraInfoSpec::BLOCKING);

  // Any invalid values lead to a bad parse.
  TestInitFromValue("invalidValue", false, 0);
  TestInitFromValue("blocking,invalidValue", false, 0);
  TestInitFromValue("invalidValue1,invalidValue2", false, 0);

  // BLOCKING and ASYNC_BLOCKING are mutually exclusive.
  TestInitFromValue("blocking,asyncBlocking", false, 0);
}

namespace {

const HeaderModificationTest_Modification::Type SET =
    HeaderModificationTest_Modification::SET;
const HeaderModificationTest_Modification::Type REMOVE =
    HeaderModificationTest_Modification::REMOVE;

HeaderModificationTest kTests[] = {
  // Check that extension 2 always wins when settings the same header.
  {
    // Headers before test.
    2, { {"header1", "value1"},
         {"header2", "value2"} },
    // Modifications in test.
    2, { {1, SET, "header1", "foo"},
         {2, SET, "header1", "bar"} },
    // Headers after test.
    2, { {"header1", "bar"},
         {"header2", "value2"} }
  },
  // Same as before in reverse execution order.
  {
    // Headers before test.
    2, { {"header1", "value1"},
         {"header2", "value2"} },
    // Modifications in test.
    2, { {2, SET, "header1", "bar"},
         {1, SET, "header1", "foo"} },
    // Headers after test.
    2, { {"header1", "bar"},
         {"header2", "value2"} }
  },
  // Check that two extensions can modify different headers that do not
  // conflict.
  {
    // Headers before test.
    2, { {"header1", "value1"},
         {"header2", "value2"} },
    // Modifications in test.
    2, { {1, SET, "header1", "foo"},
         {2, SET, "header2", "bar"} },
    // Headers after test.
    2, { {"header1", "foo"},
         {"header2", "bar"} }
  },
  // Check insert/delete conflict.
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    2, { {1, SET, "header1", "foo"},
         {2, REMOVE, "header1", NULL} },
    // Headers after test.
    0, { }
  },
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    2, { {2, REMOVE, "header1", NULL},
         {1, SET, "header1", "foo"} },
    // Headers after test.
    0, {}
  },
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    2, { {1, REMOVE, "header1", NULL},
         {2, SET, "header1", "foo"} },
    // Headers after test.
    1, { {"header1", "foo"} }
  },
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    2, { {2, SET, "header1", "foo"},
         {1, REMOVE, "header1", NULL} },
    // Headers after test.
    1, { {"header1", "foo"} }
  },
  // Check that edits are atomic (i.e. either all edit requests of an
  // extension are executed or none).
  {
    // Headers before test.
    0, { },
    // Modifications in test.
    3, { {1, SET, "header1", "value1"},
         {1, SET, "header2", "value2"},
         {2, SET, "header1", "foo"} },
    // Headers after test.
    1, { {"header1", "foo"} }  // set(header2) is ignored
  },
  // Check that identical edits do not conflict (set(header2) would be ignored
  // if set(header1) were considered a conflict).
  {
    // Headers before test.
    0, { },
    // Modifications in test.
    3, { {1, SET, "header1", "value2"},
         {1, SET, "header2", "foo"},
         {2, SET, "header1", "value2"} },
    // Headers after test.
    2, { {"header1", "value2"},
         {"header2", "foo"} }
  },
  // Check that identical deletes do not conflict (set(header2) would be ignored
  // if delete(header1) were considered a conflict).
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    3, { {1, REMOVE, "header1", NULL},
         {1, SET, "header2", "foo"},
         {2, REMOVE, "header1", NULL} },
    // Headers after test.
    1, { {"header2", "foo"} }
  },
  // Check that setting a value to an identical value is not considered an
  // edit operation that can conflict.
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    3, { {1, SET, "header1", "foo"},
         {1, SET, "header2", "bar"},
         {2, SET, "header1", "value1"} },
    // Headers after test.
    2, { {"header1", "foo"},
         {"header2", "bar"} }
  },
};

INSTANTIATE_TEST_CASE_P(
    ExtensionWebRequest,
    ExtensionWebRequestHeaderModificationTest,
    ::testing::ValuesIn(kTests));

}  // namespace


TEST(ExtensionWebRequestHelpersTest,
     TestInDecreasingExtensionInstallationTimeOrder) {
  linked_ptr<EventResponseDelta> a(
      new EventResponseDelta("ext_1", base::Time::FromInternalValue(0)));
  linked_ptr<EventResponseDelta> b(
      new EventResponseDelta("ext_2", base::Time::FromInternalValue(1000)));
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
  std::unique_ptr<EventResponseDelta> delta(CalculateOnBeforeRequestDelta(
      "extid", base::Time::Now(), cancel, localhost));
  ASSERT_TRUE(delta.get());
  EXPECT_TRUE(delta->cancel);
  EXPECT_EQ(localhost, delta->new_url);
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
  std::unique_ptr<EventResponseDelta> delta_added(
      CalculateOnBeforeSendHeadersDelta("extid", base::Time::Now(), cancel,
                                        &old_headers, &new_headers_added));
  ASSERT_TRUE(delta_added.get());
  EXPECT_TRUE(delta_added->cancel);
  ASSERT_TRUE(delta_added->modified_request_headers.GetHeader("key3", &value));
  EXPECT_EQ("value3", value);

  // Test deleting a header.
  net::HttpRequestHeaders new_headers_deleted;
  new_headers_deleted.SetHeader("key1", "value1");
  std::unique_ptr<EventResponseDelta> delta_deleted(
      CalculateOnBeforeSendHeadersDelta("extid", base::Time::Now(), cancel,
                                        &old_headers, &new_headers_deleted));
  ASSERT_TRUE(delta_deleted.get());
  ASSERT_EQ(1u, delta_deleted->deleted_request_headers.size());
  ASSERT_EQ("key2", delta_deleted->deleted_request_headers.front());

  // Test modifying a header.
  net::HttpRequestHeaders new_headers_modified;
  new_headers_modified.SetHeader("key1", "value1");
  new_headers_modified.SetHeader("key2", "value3");
  std::unique_ptr<EventResponseDelta> delta_modified(
      CalculateOnBeforeSendHeadersDelta("extid", base::Time::Now(), cancel,
                                        &old_headers, &new_headers_modified));
  ASSERT_TRUE(delta_modified.get());
  EXPECT_TRUE(delta_modified->deleted_request_headers.empty());
  ASSERT_TRUE(
      delta_modified->modified_request_headers.GetHeader("key2", &value));
  EXPECT_EQ("value3", value);

  // Test modifying a header if extension author just appended a new (key,
  // value) pair with a key that existed before. This is incorrect
  // usage of the API that shall be handled gracefully.
  net::HttpRequestHeaders new_headers_modified2;
  new_headers_modified2.SetHeader("key1", "value1");
  new_headers_modified2.SetHeader("key2", "value2");
  new_headers_modified2.SetHeader("key2", "value3");
  std::unique_ptr<EventResponseDelta> delta_modified2(
      CalculateOnBeforeSendHeadersDelta("extid", base::Time::Now(), cancel,
                                        &old_headers, &new_headers_modified));
  ASSERT_TRUE(delta_modified2.get());
  EXPECT_TRUE(delta_modified2->deleted_request_headers.empty());
  ASSERT_TRUE(
      delta_modified2->modified_request_headers.GetHeader("key2", &value));
  EXPECT_EQ("value3", value);
}

TEST(ExtensionWebRequestHelpersTest, TestCalculateOnHeadersReceivedDelta) {
  const bool cancel = true;
  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "Key2: Value2, Bar\r\n"
      "Key3: Value3\r\n"
      "Key5: Value5, end5\r\n"
      "X-Chrome-ID-Consistency-Response: Value6\r\n"
      "\r\n";
  scoped_refptr<net::HttpResponseHeaders> base_headers(
      new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(
            base_headers_string, sizeof(base_headers_string))));

  ResponseHeaders new_headers;
  new_headers.push_back(ResponseHeader("kEy1", "Value1"));  // Unchanged
  new_headers.push_back(ResponseHeader("Key2", "Value1"));  // Modified
  // Key3 is deleted
  new_headers.push_back(ResponseHeader("Key4", "Value4"));  // Added
  new_headers.push_back(ResponseHeader("Key5", "Value5, end5"));  // Unchanged
  new_headers.push_back(ResponseHeader("X-Chrome-ID-Consistency-Response",
                                       "Value1"));  // Modified
  GURL url;

  // The X-Chrome-ID-Consistency-Response is a protected header, but only for
  // Gaia URLs. It should be modifiable when sent from anywhere else.
  // Non-Gaia URL:
  std::unique_ptr<EventResponseDelta> delta(
      CalculateOnHeadersReceivedDelta("extid", base::Time::Now(), cancel, url,
                                      url, base_headers.get(), &new_headers));
  ASSERT_TRUE(delta.get());
  EXPECT_TRUE(delta->cancel);
  EXPECT_EQ(3u, delta->added_response_headers.size());
  EXPECT_TRUE(base::ContainsValue(delta->added_response_headers,
                                  ResponseHeader("Key2", "Value1")));
  EXPECT_TRUE(base::ContainsValue(delta->added_response_headers,
                                  ResponseHeader("Key4", "Value4")));
  EXPECT_TRUE(base::ContainsValue(
      delta->added_response_headers,
      ResponseHeader("X-Chrome-ID-Consistency-Response", "Value1")));
  EXPECT_EQ(3u, delta->deleted_response_headers.size());
  EXPECT_TRUE(base::ContainsValue(delta->deleted_response_headers,
                                  ResponseHeader("Key2", "Value2, Bar")));
  EXPECT_TRUE(base::ContainsValue(delta->deleted_response_headers,
                                  ResponseHeader("Key3", "Value3")));
  EXPECT_TRUE(base::ContainsValue(
      delta->deleted_response_headers,
      ResponseHeader("X-Chrome-ID-Consistency-Response", "Value6")));

  // Gaia URL:
  delta.reset(CalculateOnHeadersReceivedDelta(
      "extid", base::Time::Now(), cancel, GaiaUrls::GetInstance()->gaia_url(),
      url, base_headers.get(), &new_headers));
  ASSERT_TRUE(delta.get());
  EXPECT_TRUE(delta->cancel);
  EXPECT_EQ(2u, delta->added_response_headers.size());
  EXPECT_TRUE(base::ContainsValue(delta->added_response_headers,
                                  ResponseHeader("Key2", "Value1")));
  EXPECT_TRUE(base::ContainsValue(delta->added_response_headers,
                                  ResponseHeader("Key4", "Value4")));
  EXPECT_EQ(2u, delta->deleted_response_headers.size());
  EXPECT_TRUE(base::ContainsValue(delta->deleted_response_headers,
                                  ResponseHeader("Key2", "Value2, Bar")));
  EXPECT_TRUE(base::ContainsValue(delta->deleted_response_headers,
                                  ResponseHeader("Key3", "Value3")));
}

TEST(ExtensionWebRequestHelpersTest, TestCalculateOnAuthRequiredDelta) {
  const bool cancel = true;

  base::string16 username = base::ASCIIToUTF16("foo");
  base::string16 password = base::ASCIIToUTF16("bar");
  std::unique_ptr<net::AuthCredentials> credentials(
      new net::AuthCredentials(username, password));

  std::unique_ptr<EventResponseDelta> delta(CalculateOnAuthRequiredDelta(
      "extid", base::Time::Now(), cancel, &credentials));
  ASSERT_TRUE(delta.get());
  EXPECT_TRUE(delta->cancel);
  ASSERT_TRUE(delta->auth_credentials.get());
  EXPECT_EQ(username, delta->auth_credentials->username());
  EXPECT_EQ(password, delta->auth_credentials->password());
}

TEST(ExtensionWebRequestHelpersTest, TestMergeCancelOfResponses) {
  EventResponseDeltas deltas;
  TestLogger logger;
  bool canceled = false;

  // Single event that does not cancel.
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(1000)));
  d1->cancel = false;
  deltas.push_back(d1);
  MergeCancelOfResponses(deltas, &canceled, &logger);
  EXPECT_FALSE(canceled);
  EXPECT_EQ(0u, logger.log_size());

  // Second event that cancels the request
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(500)));
  d2->cancel = true;
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  MergeCancelOfResponses(deltas, &canceled, &logger);
  EXPECT_TRUE(canceled);
  EXPECT_EQ(1u, logger.log_size());
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses) {
  EventResponseDeltas deltas;
  TestLogger logger;
  helpers::IgnoredActions ignored_actions;
  GURL effective_new_url;

  // No redirect
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(0)));
  deltas.push_back(d0);
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_TRUE(effective_new_url.is_empty());

  // Single redirect.
  GURL new_url_1("http://foo.com");
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(1000)));
  d1->new_url = GURL(new_url_1);
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  logger.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(ignored_actions.empty());
  EXPECT_EQ(1u, logger.log_size());

  // Ignored redirect (due to precedence).
  GURL new_url_2("http://bar.com");
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(500)));
  d2->new_url = GURL(new_url_2);
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid2",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));
  EXPECT_EQ(2u, logger.log_size());

  // Overriding redirect.
  GURL new_url_3("http://baz.com");
  linked_ptr<EventResponseDelta> d3(
      new EventResponseDelta("extid3", base::Time::FromInternalValue(1500)));
  d3->new_url = GURL(new_url_3);
  deltas.push_back(d3);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_EQ(new_url_3, effective_new_url);
  EXPECT_EQ(2u, ignored_actions.size());
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid1",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid2",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));
  EXPECT_EQ(3u, logger.log_size());

  // Check that identical redirects don't cause a conflict.
  linked_ptr<EventResponseDelta> d4(
      new EventResponseDelta("extid4", base::Time::FromInternalValue(2000)));
  d4->new_url = GURL(new_url_3);
  deltas.push_back(d4);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_EQ(new_url_3, effective_new_url);
  EXPECT_EQ(2u, ignored_actions.size());
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid1",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid2",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));
  EXPECT_EQ(4u, logger.log_size());
}

// This tests that we can redirect to data:// urls, which is considered
// a kind of cancelling requests.
TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses2) {
  EventResponseDeltas deltas;
  TestLogger logger;
  helpers::IgnoredActions ignored_actions;
  GURL effective_new_url;

  // Single redirect.
  GURL new_url_0("http://foo.com");
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(2000)));
  d0->new_url = GURL(new_url_0);
  deltas.push_back(d0);
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_EQ(new_url_0, effective_new_url);

  // Cancel request by redirecting to a data:// URL. This shall override
  // the other redirect but not cause any conflict warnings.
  GURL new_url_1("data://foo");
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(1500)));
  d1->new_url = GURL(new_url_1);
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(ignored_actions.empty());
  EXPECT_EQ(1u, logger.log_size());

  // Cancel request by redirecting to the same data:// URL. This shall
  // not create any conflicts as it is in line with d1.
  GURL new_url_2("data://foo");
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(1000)));
  d2->new_url = GURL(new_url_2);
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();

  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(ignored_actions.empty());
  EXPECT_EQ(2u, logger.log_size());

  // Cancel redirect by redirecting to a different data:// URL. This needs
  // to create a conflict.
  GURL new_url_3("data://something_totally_different");
  linked_ptr<EventResponseDelta> d3(
      new EventResponseDelta("extid3", base::Time::FromInternalValue(500)));
  d3->new_url = GURL(new_url_3);
  deltas.push_back(d3);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(HasIgnoredAction(ignored_actions, "extid3",
                               web_request::IGNORED_ACTION_TYPE_REDIRECT));
  EXPECT_EQ(3u, logger.log_size());
}

// This tests that we can redirect to about:blank, which is considered
// a kind of cancelling requests.
TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses3) {
  EventResponseDeltas deltas;
  TestLogger logger;
  helpers::IgnoredActions ignored_actions;
  GURL effective_new_url;

  // Single redirect.
  GURL new_url_0("http://foo.com");
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(2000)));
  d0->new_url = GURL(new_url_0);
  deltas.push_back(d0);
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_EQ(new_url_0, effective_new_url);

  // Cancel request by redirecting to about:blank. This shall override
  // the other redirect but not cause any conflict warnings.
  GURL new_url_1("about:blank");
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(1500)));
  d1->new_url = GURL(new_url_1);
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  MergeOnBeforeRequestResponses(GURL(kExampleUrl), deltas, &effective_new_url,
                                &ignored_actions, &logger);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(ignored_actions.empty());
  EXPECT_EQ(1u, logger.log_size());
}

// This tests that WebSocket requests can not be redirected.
TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses4) {
  EventResponseDeltas deltas;
  TestLogger logger;
  helpers::IgnoredActions ignored_actions;
  GURL effective_new_url;

  // Single redirect.
  linked_ptr<EventResponseDelta> delta(
      new EventResponseDelta("extid", base::Time::FromInternalValue(2000)));
  delta->new_url = GURL("http://foo.com");
  deltas.push_back(delta);
  MergeOnBeforeRequestResponses(GURL("ws://example.com"), deltas,
                                &effective_new_url, &ignored_actions, &logger);
  EXPECT_EQ(GURL(), effective_new_url);
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeSendHeadersResponses) {
  net::HttpRequestHeaders base_headers;
  base_headers.SetHeader("key1", "value 1");
  base_headers.SetHeader("key2", "value 2");
  TestLogger logger;
  helpers::IgnoredActions ignored_actions;
  std::string header_value;
  EventResponseDeltas deltas;

  // Check that we can handle not changing the headers.
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(2500)));
  deltas.push_back(d0);
  bool request_headers_modified0;
  net::HttpRequestHeaders headers0;
  headers0.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(GURL(), deltas, &headers0, &ignored_actions,
                                    &logger, &request_headers_modified0);
  ASSERT_TRUE(headers0.GetHeader("key1", &header_value));
  EXPECT_EQ("value 1", header_value);
  ASSERT_TRUE(headers0.GetHeader("key2", &header_value));
  EXPECT_EQ("value 2", header_value);
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_EQ(0u, logger.log_size());
  EXPECT_FALSE(request_headers_modified0);

  // Delete, modify and add a header.
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(2000)));
  d1->deleted_request_headers.push_back("key1");
  d1->modified_request_headers.SetHeader("key2", "value 3");
  d1->modified_request_headers.SetHeader("key3", "value 3");
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  bool request_headers_modified1;
  net::HttpRequestHeaders headers1;
  headers1.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(GURL(), deltas, &headers1, &ignored_actions,
                                    &logger, &request_headers_modified1);
  EXPECT_FALSE(headers1.HasHeader("key1"));
  ASSERT_TRUE(headers1.GetHeader("key2", &header_value));
  EXPECT_EQ("value 3", header_value);
  ASSERT_TRUE(headers1.GetHeader("key3", &header_value));
  EXPECT_EQ("value 3", header_value);
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_EQ(1u, logger.log_size());
  EXPECT_TRUE(request_headers_modified1);

  // Check that conflicts are atomic, i.e. if one header modification
  // collides all other conflicts of the same extension are declined as well.
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(1500)));
  // This one conflicts:
  d2->modified_request_headers.SetHeader("key3", "value 0");
  d2->modified_request_headers.SetHeader("key4", "value 4");
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  bool request_headers_modified2;
  net::HttpRequestHeaders headers2;
  headers2.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(GURL(), deltas, &headers2, &ignored_actions,
                                    &logger, &request_headers_modified2);
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
  EXPECT_EQ(2u, logger.log_size());
  EXPECT_TRUE(request_headers_modified2);

  // Check that identical modifications don't conflict and operations
  // can be merged.
  linked_ptr<EventResponseDelta> d3(
      new EventResponseDelta("extid3", base::Time::FromInternalValue(1000)));
  d3->deleted_request_headers.push_back("key1");
  d3->modified_request_headers.SetHeader("key2", "value 3");
  d3->modified_request_headers.SetHeader("key5", "value 5");
  deltas.push_back(d3);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  bool request_headers_modified3;
  net::HttpRequestHeaders headers3;
  headers3.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(GURL(), deltas, &headers3, &ignored_actions,
                                    &logger, &request_headers_modified3);
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
  EXPECT_EQ(3u, logger.log_size());
  EXPECT_TRUE(request_headers_modified3);
}

TEST(ExtensionWebRequestHelpersTest,
     TestMergeOnBeforeSendHeadersResponses_Cookies) {
  net::HttpRequestHeaders base_headers;
  base_headers.SetHeader("Cookie",
                         "name=value; name2=value2; name3=\"value3\"");
  TestLogger logger;
  helpers::IgnoredActions ignored_actions;
  std::string header_value;
  EventResponseDeltas deltas;

  linked_ptr<RequestCookieModification> add_cookie =
      make_linked_ptr(new RequestCookieModification);
  add_cookie->type = helpers::ADD;
  add_cookie->modification.reset(new helpers::RequestCookie);
  add_cookie->modification->name.reset(new std::string("name4"));
  add_cookie->modification->value.reset(new std::string("\"value 4\""));

  linked_ptr<RequestCookieModification> add_cookie_2 =
      make_linked_ptr(new RequestCookieModification);
  add_cookie_2->type = helpers::ADD;
  add_cookie_2->modification.reset(new helpers::RequestCookie);
  add_cookie_2->modification->name.reset(new std::string("name"));
  add_cookie_2->modification->value.reset(new std::string("new value"));

  linked_ptr<RequestCookieModification> edit_cookie =
      make_linked_ptr(new RequestCookieModification);
  edit_cookie->type = helpers::EDIT;
  edit_cookie->filter.reset(new helpers::RequestCookie);
  edit_cookie->filter->name.reset(new std::string("name2"));
  edit_cookie->modification.reset(new helpers::RequestCookie);
  edit_cookie->modification->value.reset(new std::string("new value"));

  linked_ptr<RequestCookieModification> remove_cookie =
      make_linked_ptr(new RequestCookieModification);
  remove_cookie->type = helpers::REMOVE;
  remove_cookie->filter.reset(new helpers::RequestCookie);
  remove_cookie->filter->name.reset(new std::string("name3"));

  linked_ptr<RequestCookieModification> operations[] = {
      add_cookie, add_cookie_2, edit_cookie, remove_cookie
  };

  for (size_t i = 0; i < arraysize(operations); ++i) {
    linked_ptr<EventResponseDelta> delta(
        new EventResponseDelta("extid0", base::Time::FromInternalValue(i * 5)));
    delta->request_cookie_modifications.push_back(operations[i]);
    deltas.push_back(delta);
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  bool request_headers_modified1;
  net::HttpRequestHeaders headers1;
  headers1.MergeFrom(base_headers);
  ignored_actions.clear();
  MergeOnBeforeSendHeadersResponses(GURL(), deltas, &headers1, &ignored_actions,
                                    &logger, &request_headers_modified1);
  EXPECT_TRUE(headers1.HasHeader("Cookie"));
  ASSERT_TRUE(headers1.GetHeader("Cookie", &header_value));
  EXPECT_EQ("name=new value; name2=new value; name4=\"value 4\"", header_value);
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_EQ(0u, logger.log_size());
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
  TestLogger logger;
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
  scoped_refptr<net::HttpResponseHeaders> base_headers(
      new net::HttpResponseHeaders(
          net::HttpUtil::AssembleRawHeaders(
              base_headers_string.c_str(), base_headers_string.size())));

  // Check that we can handle if not touching the response headers.
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(3000)));
  deltas.push_back(d0);
  scoped_refptr<net::HttpResponseHeaders> new_headers0;
  MergeCookiesInOnHeadersReceivedResponses(GURL(), deltas, base_headers.get(),
                                           &new_headers0, &logger);
  EXPECT_FALSE(new_headers0.get());
  EXPECT_EQ(0u, logger.log_size());

  linked_ptr<ResponseCookieModification> add_cookie =
      make_linked_ptr(new ResponseCookieModification);
  add_cookie->type = helpers::ADD;
  add_cookie->modification.reset(new helpers::ResponseCookie);
  add_cookie->modification->name.reset(new std::string("name4"));
  add_cookie->modification->value.reset(new std::string("\"value4\""));

  linked_ptr<ResponseCookieModification> edit_cookie =
      make_linked_ptr(new ResponseCookieModification);
  edit_cookie->type = helpers::EDIT;
  edit_cookie->filter.reset(new helpers::FilterResponseCookie);
  edit_cookie->filter->name.reset(new std::string("name2"));
  edit_cookie->modification.reset(new helpers::ResponseCookie);
  edit_cookie->modification->value.reset(new std::string("new value"));

  linked_ptr<ResponseCookieModification> edit_cookie_2 =
      make_linked_ptr(new ResponseCookieModification);
  edit_cookie_2->type = helpers::EDIT;
  edit_cookie_2->filter.reset(new helpers::FilterResponseCookie);
  edit_cookie_2->filter->secure.reset(new bool(false));
  edit_cookie_2->modification.reset(new helpers::ResponseCookie);
  edit_cookie_2->modification->secure.reset(new bool(true));

  // Tests 'ageLowerBound' filter when cookie lifetime is set
  // in cookie's 'max-age' attribute and its value is greater than
  // the filter's value.
  linked_ptr<ResponseCookieModification> edit_cookie_3 =
      make_linked_ptr(new ResponseCookieModification);
  edit_cookie_3->type = helpers::EDIT;
  edit_cookie_3->filter.reset(new helpers::FilterResponseCookie);
  edit_cookie_3->filter->name.reset(new std::string("lBound1"));
  edit_cookie_3->filter->age_lower_bound.reset(new int(600));
  edit_cookie_3->modification.reset(new helpers::ResponseCookie);
  edit_cookie_3->modification->value.reset(new std::string("greater_1"));

  // Cookie lifetime is set in the cookie's 'expires' attribute.
  linked_ptr<ResponseCookieModification> edit_cookie_4 =
      make_linked_ptr(new ResponseCookieModification);
  edit_cookie_4->type = helpers::EDIT;
  edit_cookie_4->filter.reset(new helpers::FilterResponseCookie);
  edit_cookie_4->filter->name.reset(new std::string("lBound2"));
  edit_cookie_4->filter->age_lower_bound.reset(new int(600));
  edit_cookie_4->modification.reset(new helpers::ResponseCookie);
  edit_cookie_4->modification->value.reset(new std::string("greater_2"));

  // Tests equality of the cookie lifetime with the filter value when
  // lifetime is set in the cookie's 'max-age' attribute.
  // Note: we don't test the equality when the lifetime is set in the 'expires'
  // attribute because the tests will be flaky. The reason is calculations will
  // depend on fetching the current time.
  linked_ptr<ResponseCookieModification> edit_cookie_5 =
      make_linked_ptr(new ResponseCookieModification);
  edit_cookie_5->type = helpers::EDIT;
  edit_cookie_5->filter.reset(new helpers::FilterResponseCookie);
  edit_cookie_5->filter->name.reset(new std::string("lBound3"));
  edit_cookie_5->filter->age_lower_bound.reset(new int(2000));
  edit_cookie_5->modification.reset(new helpers::ResponseCookie);
  edit_cookie_5->modification->value.reset(new std::string("equal_2"));

  // Tests 'ageUpperBound' filter when cookie lifetime is set
  // in cookie's 'max-age' attribute and its value is lower than
  // the filter's value.
  linked_ptr<ResponseCookieModification> edit_cookie_6 =
      make_linked_ptr(new ResponseCookieModification);
  edit_cookie_6->type = helpers::EDIT;
  edit_cookie_6->filter.reset(new helpers::FilterResponseCookie);
  edit_cookie_6->filter->name.reset(new std::string("uBound1"));
  edit_cookie_6->filter->age_upper_bound.reset(new int(2000));
  edit_cookie_6->modification.reset(new helpers::ResponseCookie);
  edit_cookie_6->modification->value.reset(new std::string("smaller_1"));

  // Cookie lifetime is set in the cookie's 'expires' attribute.
  linked_ptr<ResponseCookieModification> edit_cookie_7 =
      make_linked_ptr(new ResponseCookieModification);
  edit_cookie_7->type = helpers::EDIT;
  edit_cookie_7->filter.reset(new helpers::FilterResponseCookie);
  edit_cookie_7->filter->name.reset(new std::string("uBound2"));
  edit_cookie_7->filter->age_upper_bound.reset(new int(2000));
  edit_cookie_7->modification.reset(new helpers::ResponseCookie);
  edit_cookie_7->modification->value.reset(new std::string("smaller_2"));

  // Tests equality of the cookie lifetime with the filter value when
  // lifetime is set in the cookie's 'max-age' attribute.
  linked_ptr<ResponseCookieModification> edit_cookie_8 =
      make_linked_ptr(new ResponseCookieModification);
  edit_cookie_8->type = helpers::EDIT;
  edit_cookie_8->filter.reset(new helpers::FilterResponseCookie);
  edit_cookie_8->filter->name.reset(new std::string("uBound3"));
  edit_cookie_8->filter->age_upper_bound.reset(new int(2000));
  edit_cookie_8->modification.reset(new helpers::ResponseCookie);
  edit_cookie_8->modification->value.reset(new std::string("equal_4"));

  // Tests 'ageUpperBound' filter when cookie lifetime is greater
  // than the filter value. No modification is expected to be applied.
  linked_ptr<ResponseCookieModification> edit_cookie_9 =
      make_linked_ptr(new ResponseCookieModification);
  edit_cookie_9->type = helpers::EDIT;
  edit_cookie_9->filter.reset(new helpers::FilterResponseCookie);
  edit_cookie_9->filter->name.reset(new std::string("uBound4"));
  edit_cookie_9->filter->age_upper_bound.reset(new int(2501));
  edit_cookie_9->modification.reset(new helpers::ResponseCookie);
  edit_cookie_9->modification->value.reset(new std::string("Will not change"));

  // Tests 'ageUpperBound' filter when both 'max-age' and 'expires' cookie
  // attributes are provided. 'expires' value matches the filter, however
  // no modification to the cookie is expected because 'max-age' overrides
  // 'expires' and it does not match the filter.
  linked_ptr<ResponseCookieModification> edit_cookie_10 =
      make_linked_ptr(new ResponseCookieModification);
  edit_cookie_10->type = helpers::EDIT;
  edit_cookie_10->filter.reset(new helpers::FilterResponseCookie);
  edit_cookie_10->filter->name.reset(new std::string("uBound5"));
  edit_cookie_10->filter->age_upper_bound.reset(new int(800));
  edit_cookie_10->modification.reset(new helpers::ResponseCookie);
  edit_cookie_10->modification->value.reset(new std::string("Will not change"));

  linked_ptr<ResponseCookieModification> remove_cookie =
      make_linked_ptr(new ResponseCookieModification);
  remove_cookie->type = helpers::REMOVE;
  remove_cookie->filter.reset(new helpers::FilterResponseCookie);
  remove_cookie->filter->name.reset(new std::string("name3"));

  linked_ptr<ResponseCookieModification> remove_cookie_2 =
      make_linked_ptr(new ResponseCookieModification);
  remove_cookie_2->type = helpers::REMOVE;
  remove_cookie_2->filter.reset(new helpers::FilterResponseCookie);
  remove_cookie_2->filter->name.reset(new std::string("uBound6"));
  remove_cookie_2->filter->age_upper_bound.reset(new int(700));

  linked_ptr<ResponseCookieModification> remove_cookie_3 =
      make_linked_ptr(new ResponseCookieModification);
  remove_cookie_3->type = helpers::REMOVE;
  remove_cookie_3->filter.reset(new helpers::FilterResponseCookie);
  remove_cookie_3->filter->name.reset(new std::string("sessionCookie"));
  remove_cookie_3->filter->session_cookie.reset(new bool(true));

  linked_ptr<ResponseCookieModification> remove_cookie_4 =
        make_linked_ptr(new ResponseCookieModification);
  remove_cookie_4->type = helpers::REMOVE;
  remove_cookie_4->filter.reset(new helpers::FilterResponseCookie);
  remove_cookie_4->filter->name.reset(new std::string("sessionCookie2"));
  remove_cookie_4->filter->session_cookie.reset(new bool(true));

  linked_ptr<ResponseCookieModification> operations[] = {
      add_cookie, edit_cookie, edit_cookie_2, edit_cookie_3, edit_cookie_4,
      edit_cookie_5, edit_cookie_6, edit_cookie_7, edit_cookie_8,
      edit_cookie_9, edit_cookie_10, remove_cookie, remove_cookie_2,
      remove_cookie_3, remove_cookie_4
  };

  for (size_t i = 0; i < arraysize(operations); ++i) {
    linked_ptr<EventResponseDelta> delta(
        new EventResponseDelta("extid0", base::Time::FromInternalValue(i * 5)));
    delta->response_cookie_modifications.push_back(operations[i]);
    deltas.push_back(delta);
  }
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  scoped_refptr<net::HttpResponseHeaders> headers1(
      new net::HttpResponseHeaders(
          net::HttpUtil::AssembleRawHeaders(
              base_headers_string.c_str(), base_headers_string.size())));
  scoped_refptr<net::HttpResponseHeaders> new_headers1;
  MergeCookiesInOnHeadersReceivedResponses(GURL(), deltas, headers1.get(),
                                           &new_headers1, &logger);

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
  EXPECT_EQ(0u, logger.log_size());
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnHeadersReceivedResponses) {
  TestLogger logger;
  helpers::IgnoredActions ignored_actions;
  std::string header_value;
  EventResponseDeltas deltas;

  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "Key2: Value2, Foo\r\n"
      "\r\n";
  scoped_refptr<net::HttpResponseHeaders> base_headers(
      new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(
            base_headers_string, sizeof(base_headers_string))));

  // Check that we can handle if not touching the response headers.
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(3000)));
  deltas.push_back(d0);
  bool response_headers_modified0;
  scoped_refptr<net::HttpResponseHeaders> new_headers0;
  GURL allowed_unsafe_redirect_url0;
  MergeOnHeadersReceivedResponses(GURL(kExampleUrl), deltas, base_headers.get(),
                                  &new_headers0, &allowed_unsafe_redirect_url0,
                                  &ignored_actions, &logger,
                                  &response_headers_modified0);
  EXPECT_FALSE(new_headers0.get());
  EXPECT_TRUE(allowed_unsafe_redirect_url0.is_empty());
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_EQ(0u, logger.log_size());
  EXPECT_FALSE(response_headers_modified0);

  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(2000)));
  d1->deleted_response_headers.push_back(ResponseHeader("KEY1", "Value1"));
  d1->deleted_response_headers.push_back(ResponseHeader("KEY2", "Value2, Foo"));
  d1->added_response_headers.push_back(ResponseHeader("Key2", "Value3"));
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  bool response_headers_modified1;
  scoped_refptr<net::HttpResponseHeaders> new_headers1;
  GURL allowed_unsafe_redirect_url1;
  MergeOnHeadersReceivedResponses(GURL(kExampleUrl), deltas, base_headers.get(),
                                  &new_headers1, &allowed_unsafe_redirect_url1,
                                  &ignored_actions, &logger,
                                  &response_headers_modified1);
  ASSERT_TRUE(new_headers1.get());
  EXPECT_TRUE(allowed_unsafe_redirect_url1.is_empty());
  std::multimap<std::string, std::string> expected1;
  expected1.insert(std::pair<std::string, std::string>("Key2", "Value3"));
  size_t iter = 0;
  std::string name;
  std::string value;
  std::multimap<std::string, std::string> actual1;
  while (new_headers1->EnumerateHeaderLines(&iter, &name, &value)) {
    actual1.insert(std::pair<std::string, std::string>(name, value));
  }
  EXPECT_EQ(expected1, actual1);
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_EQ(1u, logger.log_size());
  EXPECT_TRUE(response_headers_modified1);

  // Check that we replace response headers only once.
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(1500)));
  // Note that we use a different capitalization of KeY2. This should not
  // matter.
  d2->deleted_response_headers.push_back(ResponseHeader("KeY2", "Value2, Foo"));
  d2->added_response_headers.push_back(ResponseHeader("Key2", "Value4"));
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  bool response_headers_modified2;
  scoped_refptr<net::HttpResponseHeaders> new_headers2;
  GURL allowed_unsafe_redirect_url2;
  MergeOnHeadersReceivedResponses(GURL(kExampleUrl), deltas, base_headers.get(),
                                  &new_headers2, &allowed_unsafe_redirect_url2,
                                  &ignored_actions, &logger,
                                  &response_headers_modified2);
  ASSERT_TRUE(new_headers2.get());
  EXPECT_TRUE(allowed_unsafe_redirect_url2.is_empty());
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
  EXPECT_EQ(2u, logger.log_size());
  EXPECT_TRUE(response_headers_modified2);
}

// Check that we do not delete too much
TEST(ExtensionWebRequestHelpersTest,
     TestMergeOnHeadersReceivedResponsesDeletion) {
  TestLogger logger;
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
  scoped_refptr<net::HttpResponseHeaders> base_headers(
      new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(
            base_headers_string, sizeof(base_headers_string))));

  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(2000)));
  d1->deleted_response_headers.push_back(ResponseHeader("KEY1", "Value2"));
  deltas.push_back(d1);
  bool response_headers_modified1;
  scoped_refptr<net::HttpResponseHeaders> new_headers1;
  GURL allowed_unsafe_redirect_url1;
  MergeOnHeadersReceivedResponses(GURL(kExampleUrl), deltas, base_headers.get(),
                                  &new_headers1, &allowed_unsafe_redirect_url1,
                                  &ignored_actions, &logger,
                                  &response_headers_modified1);
  ASSERT_TRUE(new_headers1.get());
  EXPECT_TRUE(allowed_unsafe_redirect_url1.is_empty());
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
  EXPECT_EQ(1u, logger.log_size());
  EXPECT_TRUE(response_headers_modified1);
}

// Tests whether onHeadersReceived can initiate a redirect.
// The URL merge logic is shared with onBeforeRequest, so we only need to test
// whether the URLs are merged at all.
TEST(ExtensionWebRequestHelpersTest,
     TestMergeOnHeadersReceivedResponsesRedirect) {
  EventResponseDeltas deltas;
  TestLogger logger;
  helpers::IgnoredActions ignored_actions;

  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "\r\n";
  scoped_refptr<net::HttpResponseHeaders> base_headers(
      new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
          base_headers_string, sizeof(base_headers_string))));

  // No redirect
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(0)));
  deltas.push_back(d0);
  bool response_headers_modified0;
  scoped_refptr<net::HttpResponseHeaders> new_headers0;
  GURL allowed_unsafe_redirect_url0;
  MergeOnHeadersReceivedResponses(GURL(kExampleUrl), deltas, base_headers.get(),
                                  &new_headers0, &allowed_unsafe_redirect_url0,
                                  &ignored_actions, &logger,
                                  &response_headers_modified0);
  EXPECT_FALSE(new_headers0.get());
  EXPECT_TRUE(allowed_unsafe_redirect_url0.is_empty());
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_EQ(0u, logger.log_size());
  EXPECT_FALSE(response_headers_modified0);

  // Single redirect.
  GURL new_url_1("http://foo.com");
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(1000)));
  d1->new_url = GURL(new_url_1);
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  logger.clear();
  bool response_headers_modified1;

  scoped_refptr<net::HttpResponseHeaders> new_headers1;
  GURL allowed_unsafe_redirect_url1;
  MergeOnHeadersReceivedResponses(GURL(kExampleUrl), deltas, base_headers.get(),
                                  &new_headers1, &allowed_unsafe_redirect_url1,
                                  &ignored_actions, &logger,
                                  &response_headers_modified1);

  EXPECT_TRUE(new_headers1.get());
  EXPECT_TRUE(new_headers1->HasHeaderValue("Location", new_url_1.spec()));
  EXPECT_EQ(new_url_1, allowed_unsafe_redirect_url1);
  EXPECT_TRUE(ignored_actions.empty());
  EXPECT_EQ(1u, logger.log_size());
  EXPECT_FALSE(response_headers_modified1);
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnAuthRequiredResponses) {
  TestLogger logger;
  helpers::IgnoredActions ignored_actions;
  EventResponseDeltas deltas;
  base::string16 username = base::ASCIIToUTF16("foo");
  base::string16 password = base::ASCIIToUTF16("bar");
  base::string16 password2 = base::ASCIIToUTF16("baz");

  // Check that we can handle if not returning credentials.
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(3000)));
  deltas.push_back(d0);
  net::AuthCredentials auth0;
  bool credentials_set =
      MergeOnAuthRequiredResponses(deltas, &auth0, &ignored_actions, &logger);
  EXPECT_FALSE(credentials_set);
  EXPECT_TRUE(auth0.Empty());
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_EQ(0u, logger.log_size());

  // Check that we can set AuthCredentials.
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(2000)));
  d1->auth_credentials.reset(new net::AuthCredentials(username, password));
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  net::AuthCredentials auth1;
  credentials_set =
      MergeOnAuthRequiredResponses(deltas, &auth1, &ignored_actions, &logger);
  EXPECT_TRUE(credentials_set);
  EXPECT_FALSE(auth1.Empty());
  EXPECT_EQ(username, auth1.username());
  EXPECT_EQ(password, auth1.password());
  EXPECT_EQ(0u, ignored_actions.size());
  EXPECT_EQ(1u, logger.log_size());

  // Check that we set AuthCredentials only once.
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(1500)));
  d2->auth_credentials.reset(new net::AuthCredentials(username, password2));
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  net::AuthCredentials auth2;
  credentials_set =
      MergeOnAuthRequiredResponses(deltas, &auth2, &ignored_actions, &logger);
  EXPECT_TRUE(credentials_set);
  EXPECT_FALSE(auth2.Empty());
  EXPECT_EQ(username, auth1.username());
  EXPECT_EQ(password, auth1.password());
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid2",
                       web_request::IGNORED_ACTION_TYPE_AUTH_CREDENTIALS));
  EXPECT_EQ(2u, logger.log_size());

  // Check that we can set identical AuthCredentials twice without causing
  // a conflict.
  linked_ptr<EventResponseDelta> d3(
      new EventResponseDelta("extid3", base::Time::FromInternalValue(1000)));
  d3->auth_credentials.reset(new net::AuthCredentials(username, password));
  deltas.push_back(d3);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  ignored_actions.clear();
  logger.clear();
  net::AuthCredentials auth3;
  credentials_set =
      MergeOnAuthRequiredResponses(deltas, &auth3, &ignored_actions, &logger);
  EXPECT_TRUE(credentials_set);
  EXPECT_FALSE(auth3.Empty());
  EXPECT_EQ(username, auth1.username());
  EXPECT_EQ(password, auth1.password());
  EXPECT_EQ(1u, ignored_actions.size());
  EXPECT_TRUE(
      HasIgnoredAction(ignored_actions, "extid2",
                       web_request::IGNORED_ACTION_TYPE_AUTH_CREDENTIALS));
  EXPECT_EQ(3u, logger.log_size());
}

}  // namespace extensions
