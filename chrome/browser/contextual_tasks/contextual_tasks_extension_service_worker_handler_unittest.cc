// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_service_worker_handler.h"

#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_page.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/common/logger.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "url/gurl.h"

namespace contextual_tasks {
namespace {

struct RecordedLogEntry {
  std::string tag;
  std::string message;
  std::optional<std::string> proto_type;
  std::optional<std::string> proto_base64;
};

class RecordingOmniboxLoggerObserver : public omnibox::Logger::Observer {
 public:
  RecordingOmniboxLoggerObserver() {
    observation_.Observe(omnibox::Logger::GetInstance());
  }
  ~RecordingOmniboxLoggerObserver() override = default;

  void OnLogMessageAdded(
      base::Time event_time,
      const std::string& tag,
      const std::string& source_file,
      uint32_t source_line,
      const std::string& message,
      const std::optional<std::string>& proto_type,
      const std::optional<std::string>& proto_base64) override {
    entries_.push_back(RecordedLogEntry{
        .tag = tag,
        .message = message,
        .proto_type = proto_type,
        .proto_base64 = proto_base64,
    });
  }

  const std::vector<RecordedLogEntry>& entries() const { return entries_; }
  void Clear() { entries_.clear(); }

 private:
  std::vector<RecordedLogEntry> entries_;
  base::ScopedObservation<omnibox::Logger, omnibox::Logger::Observer>
      observation_{this};
};

template <typename ProtoType>
std::vector<uint8_t> SerializeProto(const ProtoType& proto) {
  std::vector<uint8_t> bytes(proto.ByteSizeLong());
  proto.SerializeToArray(bytes.data(), bytes.size());
  return bytes;
}

class ContextualTasksExtensionServiceWorkerHandlerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(kContextualTasksRearchitecture);
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://www.google.com/search?udm=50"));

    handler_ = std::make_unique<ContextualTasksExtensionServiceWorkerHandler>(
        *profile());
    handler_->CreateExtensionPageHandler(
        mock_page_.BindAndGetRemote(),
        handler_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    handler_remote_.reset();
    handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::string GetMainFrameDocumentId() {
    return extensions::ExtensionApiFrameIdMap::GetDocumentId(main_rfh())
        .ToString();
  }

  base::test::ScopedFeatureList feature_list_;
  testing::NiceMock<MockContextualTasksExtensionPage> mock_page_;
  mojo::Remote<mojom::ExtensionPageHandler> handler_remote_;
  std::unique_ptr<ContextualTasksExtensionServiceWorkerHandler> handler_;
};

TEST_F(ContextualTasksExtensionServiceWorkerHandlerTest,
       LogsHandshakeAndBidirectionalSearchMessagesDistinctFromIframeWebview) {
  RecordingOmniboxLoggerObserver log_observer;
  const std::string document_id = GetMainFrameDocumentId();

  // 1. Outbound HandshakePing via Service Worker.
  base::test::TestFuture<mojo_base::ProtoWrapper> handshake_future;
  handler_->GetHandshakeMessage(document_id, handshake_future.GetCallback());
  mojo_base::ProtoWrapper handshake_wrapper = handshake_future.Take();
  std::optional<lens::ClientToAimMessage> returned_handshake =
      handshake_wrapper.As<lens::ClientToAimMessage>();
  ASSERT_TRUE(returned_handshake.has_value());
  EXPECT_TRUE(returned_handshake->has_handshake_ping());

  ASSERT_EQ(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "PostServiceWorkerMessage");
  EXPECT_THAT(log_observer.entries()[0].message,
              testing::AllOf(testing::HasSubstr("HandshakePing"),
                             testing::HasSubstr(document_id)));
  EXPECT_EQ(log_observer.entries()[0].proto_type,
            "lens.chrome.ClientToAimMessage");
  ASSERT_TRUE(log_observer.entries()[0].proto_base64.has_value());
  std::string decoded_handshake;
  ASSERT_TRUE(base::Base64Decode(*log_observer.entries()[0].proto_base64,
                                 &decoded_handshake));
  lens::ClientToAimMessage parsed_handshake;
  ASSERT_TRUE(parsed_handshake.ParseFromString(decoded_handshake));
  EXPECT_TRUE(parsed_handshake.has_handshake_ping());

  log_observer.Clear();

  // Stage a pending outbound message before handshake completes to verify
  // inbound HandshakeResponse is logged BEFORE flushed outbound messages and
  // delivered over the Mojo ExtensionPage remote.
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents());
  lens::ClientToSearchMessage open_threads_msg;
  open_threads_msg.mutable_open_threads_view();
  user_data->PostSearchMessage(open_threads_msg);
  EXPECT_TRUE(log_observer.entries().empty());

  EXPECT_CALL(mock_page_, PostSearchMessageWithDocumentId(
                              testing::_, testing::Optional(document_id)))
      .Times(1);

  // 2. Inbound HandshakeResponse via Service Worker flushes outbound
  // OpenThreadsView in chronological order.
  lens::SearchToClientMessage handshake_response;
  handshake_response.mutable_handshake_response()->set_auth_user_index(1);
  handler_->OnWebviewMessage(SerializeProto(handshake_response), document_id);
  mock_page_.FlushForTesting();

  ASSERT_GE(log_observer.entries().size(), 2u);
  EXPECT_EQ(log_observer.entries()[0].tag, "OnServiceWorkerMessage");
  EXPECT_THAT(log_observer.entries()[0].message,
              testing::AllOf(testing::HasSubstr("HandshakeResponse"),
                             testing::HasSubstr(document_id)));
  EXPECT_EQ(log_observer.entries()[0].proto_type,
            "lens.chrome.SearchToClientMessage");
  ASSERT_TRUE(log_observer.entries()[0].proto_base64.has_value());
  std::string decoded_inbound;
  ASSERT_TRUE(base::Base64Decode(*log_observer.entries()[0].proto_base64,
                                 &decoded_inbound));
  lens::SearchToClientMessage parsed_inbound;
  ASSERT_TRUE(parsed_inbound.ParseFromString(decoded_inbound));
  EXPECT_EQ(parsed_inbound.handshake_response().auth_user_index(), 1);

  EXPECT_EQ(log_observer.entries()[1].tag, "PostServiceWorkerMessage");
  EXPECT_THAT(log_observer.entries()[1].message,
              testing::AllOf(testing::HasSubstr("OpenThreadsView"),
                             testing::HasSubstr(document_id),
                             testing::Not(testing::HasSubstr("dropped"))));
  EXPECT_EQ(log_observer.entries()[1].proto_type,
            "lens.chrome.ClientToSearchMessage");

  // 3. Inbound OnPageChanged via Service Worker.
  log_observer.Clear();
  lens::SearchToClientMessage page_changed;
  page_changed.mutable_on_page_changed()->set_is_zero_state(true);
  handler_->OnWebviewMessage(SerializeProto(page_changed), document_id);

  ASSERT_GE(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "OnServiceWorkerMessage");
  EXPECT_THAT(log_observer.entries()[0].message,
              testing::AllOf(testing::HasSubstr("OnPageChanged"),
                             testing::HasSubstr(document_id)));
  EXPECT_EQ(log_observer.entries()[0].proto_type,
            "lens.chrome.SearchToClientMessage");

  // Ensure none of the Service Worker logs reuse legacy webview/iframe tags.
  for (const auto& entry : log_observer.entries()) {
    EXPECT_NE(entry.tag, "OnWebviewMessage");
    EXPECT_NE(entry.tag, "PostAimMessage");
  }
}

TEST_F(ContextualTasksExtensionServiceWorkerHandlerTest,
       LogsDroppedAndRejectedServiceWorkerMessages) {
  RecordingOmniboxLoggerObserver log_observer;
  const std::string document_id = GetMainFrameDocumentId();

  // 1. Rejected handshake with missing document_id.
  base::test::TestFuture<mojo_base::ProtoWrapper> missing_doc_future;
  handler_->GetHandshakeMessage(std::nullopt, missing_doc_future.GetCallback());
  (void)missing_doc_future.Take();
  ASSERT_EQ(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "PostServiceWorkerMessage");
  EXPECT_THAT(
      log_observer.entries()[0].message,
      testing::HasSubstr("HandshakePing rejected: missing document_id"));

  // 2. Rejected handshake with invalid document_id.
  log_observer.Clear();
  base::test::TestFuture<mojo_base::ProtoWrapper> invalid_doc_future;
  handler_->GetHandshakeMessage("invalid-doc-id",
                                invalid_doc_future.GetCallback());
  (void)invalid_doc_future.Take();
  ASSERT_EQ(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "PostServiceWorkerMessage");
  EXPECT_THAT(log_observer.entries()[0].message,
              testing::HasSubstr("HandshakePing rejected: invalid frame"));

  // 3. Dropped inbound messages with missing document_id, empty payload,
  // invalid frame, and malformed bytes.
  log_observer.Clear();
  handler_->OnWebviewMessage({0x0a, 0x00}, std::nullopt);
  ASSERT_EQ(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "OnServiceWorkerMessage");
  EXPECT_THAT(
      log_observer.entries()[0].message,
      testing::HasSubstr("SearchToClientMessage dropped: missing document_id"));

  log_observer.Clear();
  handler_->OnWebviewMessage({}, document_id);
  ASSERT_EQ(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "OnServiceWorkerMessage");
  EXPECT_THAT(
      log_observer.entries()[0].message,
      testing::HasSubstr("SearchToClientMessage dropped: invalid size"));

  log_observer.Clear();
  handler_->OnWebviewMessage({0x0a, 0x00}, "invalid-doc-id");
  ASSERT_EQ(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "OnServiceWorkerMessage");
  EXPECT_THAT(
      log_observer.entries()[0].message,
      testing::HasSubstr("SearchToClientMessage dropped: invalid frame"));

  log_observer.Clear();
  handler_->OnWebviewMessage({0xff, 0xfe, 0xfd}, document_id);
  ASSERT_EQ(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "OnServiceWorkerMessage");
  EXPECT_THAT(
      log_observer.entries()[0].message,
      testing::HasSubstr("SearchToClientMessage dropped: failed to parse"));

  // 4. Dropped outbound ClientToSearchMessage when ExtensionPage remote is
  // unbound.
  log_observer.Clear();
  ContextualTasksExtensionServiceWorkerHandler unbound_handler(*profile());
  lens::SearchToClientMessage handshake_response;
  handshake_response.mutable_handshake_response()->set_auth_user_index(0);
  unbound_handler.OnWebviewMessage(SerializeProto(handshake_response),
                                   document_id);

  log_observer.Clear();
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents());
  lens::ClientToSearchMessage open_threads_msg;
  open_threads_msg.mutable_open_threads_view();
  user_data->PostSearchMessage(open_threads_msg);
  ASSERT_EQ(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "PostServiceWorkerMessage");
  EXPECT_THAT(
      log_observer.entries()[0].message,
      testing::HasSubstr("OpenThreadsView dropped: page remote unbound"));
  EXPECT_EQ(log_observer.entries()[0].proto_type,
            "lens.chrome.ClientToSearchMessage");

  // 5. Feature-disabled rejection/drop paths.
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kContextualTasksRearchitecture);

  log_observer.Clear();
  base::test::TestFuture<mojo_base::ProtoWrapper> disabled_future;
  handler_->GetHandshakeMessage(document_id, disabled_future.GetCallback());
  (void)disabled_future.Take();
  ASSERT_EQ(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "PostServiceWorkerMessage");
  EXPECT_THAT(log_observer.entries()[0].message,
              testing::HasSubstr("HandshakePing rejected: feature disabled"));

  log_observer.Clear();
  handler_->OnWebviewMessage(SerializeProto(handshake_response), document_id);
  ASSERT_EQ(log_observer.entries().size(), 1u);
  EXPECT_EQ(log_observer.entries()[0].tag, "OnServiceWorkerMessage");
  EXPECT_THAT(
      log_observer.entries()[0].message,
      testing::HasSubstr("SearchToClientMessage dropped: feature disabled"));
}

TEST_F(ContextualTasksExtensionServiceWorkerHandlerTest,
       RecordsHandshakeAndErrorHistograms) {
  base::HistogramTester histogram_tester;
  const std::string document_id = GetMainFrameDocumentId();

  // 1. Start handshake and complete it: records kSuccess and Latency, and
  // records 0 dropped queued messages when the page navigates away.
  base::test::TestFuture<mojo_base::ProtoWrapper> handshake_future;
  handler_->GetHandshakeMessage(document_id, handshake_future.GetCallback());
  (void)handshake_future.Take();

  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents());
  lens::ClientToSearchMessage queued_msg;
  queued_msg.mutable_open_threads_view();
  user_data->PostSearchMessage(queued_msg);

  lens::SearchToClientMessage handshake_response;
  handshake_response.mutable_handshake_response()->set_auth_user_index(0);
  handler_->OnWebviewMessage(SerializeProto(handshake_response), document_id);
  mock_page_.FlushForTesting();

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ExtensionHandshake.Status",
      ExtensionHandshakeStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ExtensionHandshake.Latency", 1);

  // Navigate to a new page to tear down PageSearchState; since the queued
  // message was flushed on handshake completion, 0 dropped messages should be
  // recorded.
  NavigateAndCommit(GURL("https://example.com/next"));
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ExtensionPostMessage.QueuedSearchMessagesDropped", 0, 1);

  // 2. Start a second handshake on the new page, queue 2 search messages, and
  // navigate away before the handshake completes: records kFrameDetached and 2
  // dropped queued messages.
  const std::string second_doc_id = GetMainFrameDocumentId();
  base::test::TestFuture<mojo_base::ProtoWrapper> second_handshake_future;
  handler_->GetHandshakeMessage(second_doc_id,
                                second_handshake_future.GetCallback());
  (void)second_handshake_future.Take();

  user_data->PostSearchMessage(queued_msg);
  user_data->PostSearchMessage(queued_msg);

  NavigateAndCommit(GURL("https://www.google.com/search?udm=50&q=third"));
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.ExtensionHandshake.Status",
      ExtensionHandshakeStatus::kFrameDetached, 1);
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.ExtensionPostMessage.QueuedSearchMessagesDropped", 2, 1);

  // 3. Invalid document_id handshakes record kInvalidDocumentId.
  base::test::TestFuture<mojo_base::ProtoWrapper> invalid_handshake_future;
  handler_->GetHandshakeMessage("invalid-doc-id",
                                invalid_handshake_future.GetCallback());
  (void)invalid_handshake_future.Take();
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.ExtensionHandshake.Status",
      ExtensionHandshakeStatus::kInvalidDocumentId, 1);

  // 4. Dropped inbound/outbound messages record ExtensionPostMessage.Error.
  const std::string third_doc_id = GetMainFrameDocumentId();
  handler_->OnWebviewMessage({0x0a, 0x00}, std::nullopt);
  handler_->OnWebviewMessage({0x0a, 0x00}, "invalid-doc-id");
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.ExtensionPostMessage.Error",
      ExtensionPostMessageError::kInvalidDocumentId, 2);

  handler_->OnWebviewMessage({}, third_doc_id);
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.ExtensionPostMessage.Error",
      ExtensionPostMessageError::kUnexpectedDataType, 1);

  std::vector<uint8_t> oversized(1024 * 1024 + 1, 0x00);
  handler_->OnWebviewMessage(oversized, third_doc_id);
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.ExtensionPostMessage.Error",
      ExtensionPostMessageError::kMessageTooLarge, 1);

  handler_->OnWebviewMessage({0xff, 0xfe, 0xfd}, third_doc_id);
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.ExtensionPostMessage.Error",
      ExtensionPostMessageError::kProtoParseFailed, 1);

  // Unbound page remote records kMojoPipeClosed on outbound dispatch.
  ContextualTasksExtensionServiceWorkerHandler unbound_handler(*profile());
  unbound_handler.OnWebviewMessage(SerializeProto(handshake_response),
                                   third_doc_id);
  user_data->PostSearchMessage(queued_msg);
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.ExtensionPostMessage.Error",
      ExtensionPostMessageError::kMojoPipeClosed, 1);
}

}  // namespace
}  // namespace contextual_tasks
