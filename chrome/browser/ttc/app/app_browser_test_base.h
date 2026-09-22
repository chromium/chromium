// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_APP_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_TTC_APP_APP_BROWSER_TEST_BASE_H_

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ttc/app/ttc_mes_client.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/features/ttc.pb.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

class MockOptimizationGuideKeyedService;
class Profile;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace media {
class MockAudioManager;
}

namespace optimization_guide {
class MockRemoteModelExecutionSession;
}  // namespace optimization_guide

namespace ttc {

class Conversation;
class ConversationImpl;
class SessionController;
class SessionControllerImpl;
class TtcKeyedService;

// Base class for Ttc app browser tests with common settings and setup. These
// tests use all real objects throughout the system with the exception of the
// backend session (via MockOptimizationGuideKeyedService) and client IO (via
// MockAudioManager).
class AppBrowserTestBase : public PlatformBrowserTest {
 public:
  AppBrowserTestBase();
  ~AppBrowserTestBase() override;

  // PlatformBrowserTest:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  Profile* profile();
  content::WebContents* web_contents();
  TtcKeyedService& ttc_service();

  MockOptimizationGuideKeyedService& optimization_guide_service();

  // The controller for the service's current session, or null if there's no
  // session.
  SessionControllerImpl* session_controller();

  // The conversation belonging to the service's current session, or null if
  // there's no session.
  ConversationImpl* conversation();

  // The backend used by the service's current session, or null if there's no
  // session.
  TtcMesClient* backend();

  // The mock streaming session the session's TtcMesClient will be, or has
  // been, given. Valid from setup until the Ttc session ends, so tests can set
  // expectations on it before calling StartSession(). Null once the Ttc
  // session has ended but can be reset with ResetModelExecutionSession().
  optimization_guide::MockRemoteModelExecutionSession*
  model_execution_session();

  // Readies a new mock streaming session for the next TtcMesClient to request
  // one. This is already done for the first session in a test but tests must
  // call this before starting a second Ttc session.
  void ResetModelExecutionSession();

  // Starts a session, ensuring the client sends its setup frame and that the
  // backend connection is opened.
  void StartSessionAndConnectBackend();

  // Simulates the backend connection opening or closing. These require a
  // session: the client only starts watching the streaming session once it has
  // one.
  void OpenBackendConnection();
  void CloseBackendConnection();

  // Sends `frame` to the client as if it came from the server. Requires that a
  // session has been started.
  void SimulateResponse(const optimization_guide::proto::TtcServerFrame& frame);

  // Sends a SessionStatusResponse to the client as if it came from the server.
  void RespondSessionStatus(
      optimization_guide::proto::SessionStatusResponse::SessionState state,
      std::string_view server_session_id,
      std::string_view diagnostic_error_detail = "");

  // Sends a ServerErrorNotification to the client as if it came from the
  // server.
  void RespondServerError(
      optimization_guide::proto::ServerErrorNotification::ErrorCode error_code,
      std::string_view error_message = "");

  // Runs until the service reaches `state`. Returns false if it timed out
  // first.
  [[nodiscard]] bool WaitForServiceState(ServiceState state);

 private:
  std::unique_ptr<Conversation> MakeConversation(
      SessionController& session_controller);

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<media::MockAudioManager> audio_manager_;

  // Holds the mock streaming session until the TtcMesClient takes ownership of
  // it.
  std::unique_ptr<optimization_guide::RemoteModelExecutionSession>
      pending_model_execution_session_;

  // The callback the TtcMesClient passed to StartStreamingSession(). Responses
  // from the server are delivered by invoking it.
  optimization_guide::OptimizationGuideModelExecutionStreamingCallback
      streaming_callback_;
};

}  // namespace ttc

// Expects `method` to be called once on the session's mock model execution
// session and records the client frame it was called with into `future`:
//
//   base::test::TestFuture<optimization_guide::proto::TtcClientFrame> frame;
//   EXPECT_CLIENT_FRAME(Send, frame);
//   ttc_service().StartSession();
//   EXPECT_TRUE(frame.Get().has_setup_request());
//
#define EXPECT_CLIENT_FRAME(method, future)                                 \
  EXPECT_CALL(*model_execution_session(), method)                           \
      .WillOnce([callback = future.GetCallback()](                          \
                    const google::protobuf::MessageLite& message) mutable { \
        optimization_guide::proto::TtcClientFrame frame;                    \
        CHECK(frame.ParseFromString(message.SerializeAsString()));          \
        std::move(callback).Run(std::move(frame));                          \
      })

#endif  // CHROME_BROWSER_TTC_APP_APP_BROWSER_TEST_BASE_H_
