// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/app_browser_test_base.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/types/expected.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/mock_remote_model_executor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/audio_controller.h"
#include "chrome/browser/ttc/app/conversation_impl.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/app/public/make_conversation.h"
#include "chrome/browser/ttc/app/ttc_mes_client.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "content/public/browser/browser_context.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ttc {

namespace {

// Creates a server frame carrying a SessionStatusResponse with the given
// fields.
optimization_guide::proto::TtcServerFrame MakeSessionStatusFrame(
    optimization_guide::proto::SessionStatusResponse::SessionState state,
    std::string_view server_session_id,
    std::string_view diagnostic_error_detail) {
  optimization_guide::proto::TtcServerFrame frame;
  auto* status = frame.mutable_session_status();
  status->set_state(state);
  status->set_server_session_id(server_session_id);
  status->set_diagnostic_error_detail(diagnostic_error_detail);
  return frame;
}

// Creates a server frame carrying a ServerErrorNotification with the given
// fields.
optimization_guide::proto::TtcServerFrame MakeServerErrorFrame(
    optimization_guide::proto::ServerErrorNotification::ErrorCode error_code,
    std::string_view error_message) {
  optimization_guide::proto::TtcServerFrame frame;
  auto* error = frame.mutable_server_error();
  error->set_error_code(error_code);
  error->set_error_message(error_message);
  return frame;
}

}  // namespace

AppBrowserTestBase::AppBrowserTestBase() {
  scoped_feature_list_.InitAndEnableFeature(kTtc);
}

AppBrowserTestBase::~AppBrowserTestBase() = default;

void AppBrowserTestBase::SetUpBrowserContextKeyedServices(
    content::BrowserContext* context) {
  PlatformBrowserTest::SetUpBrowserContextKeyedServices(context);

  // The conversation uses the production TtcMesClient so mock out the service
  // it talks to rather than the client itself.
  OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating([](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            testing::NiceMock<MockOptimizationGuideKeyedService>>();
      }));

  // Replace the service with one whose sessions use the conversation created
  // by MakeConversation().
  TtcKeyedServiceFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindLambdaForTesting([this](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
        return std::make_unique<TtcKeyedService>(
            Profile::FromBrowserContext(context),
            base::BindRepeating(&AppBrowserTestBase::MakeConversation,
                                base::Unretained(this)));
      }));
}

void AppBrowserTestBase::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  audio_manager_ = std::make_unique<media::MockAudioManager>(
      std::make_unique<media::TestAudioThread>());
  audio_manager_->SetHasInputDevices(true);
  audio_manager_->SetInputStreamParameters(
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Mono(),
                             /*sample_rate=*/48000,
                             /*frames_per_buffer=*/480));

  // Created up front, rather than when requested, so that tests can set
  // expectations on it before starting a session.
  ResetModelExecutionSession();

  ON_CALL(optimization_guide_service(), StartStreamingSession)
      .WillByDefault(
          [this](optimization_guide::ModelBasedCapabilityKey,
                 const optimization_guide::StreamingModelExecutionOptions&,
                 optimization_guide::
                     OptimizationGuideModelExecutionStreamingCallback callback)
              -> std::unique_ptr<
                  optimization_guide::RemoteModelExecutionSession> {
            CHECK(pending_model_execution_session_)
                << "Call ResetModelExecutionSession() before starting another "
                   "session.";
            streaming_callback_ = std::move(callback);
            return std::move(pending_model_execution_session_);
          });
}

void AppBrowserTestBase::TearDownOnMainThread() {
  // The session owns the AudioController so it must go away before the audio
  // manager it's using.
  ttc_service().EndSession();
  pending_model_execution_session_.reset();
  streaming_callback_.Reset();

  audio_manager_->Shutdown();
  audio_manager_.reset();

  PlatformBrowserTest::TearDownOnMainThread();
}

Profile* AppBrowserTestBase::profile() {
  return chrome_test_utils::GetProfile(this);
}

content::WebContents* AppBrowserTestBase::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

TtcKeyedService& AppBrowserTestBase::ttc_service() {
  return CHECK_DEREF(TtcKeyedService::Get(profile()));
}

MockOptimizationGuideKeyedService&
AppBrowserTestBase::optimization_guide_service() {
  return CHECK_DEREF(static_cast<MockOptimizationGuideKeyedService*>(
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile())));
}

SessionControllerImpl* AppBrowserTestBase::session_controller() {
  return static_cast<SessionControllerImpl*>(
      ttc_service().session_controller());
}

ConversationImpl* AppBrowserTestBase::conversation() {
  SessionControllerImpl* controller = session_controller();
  return controller
             ? static_cast<ConversationImpl*>(&controller->conversation())
             : nullptr;
}

TtcMesClient* AppBrowserTestBase::backend() {
  ConversationImpl* conversation_impl = conversation();
  return conversation_impl
             ? static_cast<TtcMesClient*>(conversation_impl->backend())
             : nullptr;
}

optimization_guide::MockRemoteModelExecutionSession*
AppBrowserTestBase::model_execution_session() {
  // Ownership moves to the client once it starts its streaming session.
  optimization_guide::RemoteModelExecutionSession* session =
      backend() ? backend()->GetExecutionSessionForTesting() : nullptr;
  if (!session) {
    session = pending_model_execution_session_.get();
  }
  return static_cast<optimization_guide::MockRemoteModelExecutionSession*>(
      session);
}

void AppBrowserTestBase::ResetModelExecutionSession() {
  pending_model_execution_session_ = std::make_unique<
      testing::NiceMock<optimization_guide::MockRemoteModelExecutionSession>>();
}

void AppBrowserTestBase::StartSessionAndConnectBackend() {
  ttc_service().StartSession();

  // The connection is established once the client has sent its setup frame.
  OpenBackendConnection();
}

void AppBrowserTestBase::OpenBackendConnection() {
  CHECK_DEREF(model_execution_session())
      .SetConnectionState(optimization_guide::RemoteModelExecutionSession::
                              ConnectionState::kConnected);
}

void AppBrowserTestBase::CloseBackendConnection() {
  CHECK_DEREF(model_execution_session())
      .SetConnectionState(optimization_guide::RemoteModelExecutionSession::
                              ConnectionState::kDisconnected);
}

void AppBrowserTestBase::SimulateResponse(
    const optimization_guide::proto::TtcServerFrame& frame) {
  CHECK(streaming_callback_) << "No streaming session has been started.";
  streaming_callback_.Run(
      optimization_guide::OptimizationGuideModelStreamingResult(
          base::ok(optimization_guide::AnyWrapProto(frame)),
          /*execution_info=*/nullptr));
}

void AppBrowserTestBase::RespondSessionStatus(
    optimization_guide::proto::SessionStatusResponse::SessionState state,
    std::string_view server_session_id,
    std::string_view diagnostic_error_detail) {
  SimulateResponse(MakeSessionStatusFrame(state, server_session_id,
                                          diagnostic_error_detail));
}

void AppBrowserTestBase::RespondServerError(
    optimization_guide::proto::ServerErrorNotification::ErrorCode error_code,
    std::string_view error_message) {
  SimulateResponse(MakeServerErrorFrame(error_code, error_message));
}

bool AppBrowserTestBase::WaitForServiceState(ServiceState state) {
  if (ttc_service().GetState() == state) {
    return true;
  }

  base::test::TestFuture<void> reached;
  base::CallbackListSubscription subscription =
      ttc_service().RegisterStateChangedCallback(
          base::BindLambdaForTesting([&](ServiceState new_state) {
            if (new_state == state && !reached.IsReady()) {
              reached.SetValue();
            }
          }));
  return reached.Wait();
}

std::unique_ptr<Conversation> AppBrowserTestBase::MakeConversation(
    SessionController& session_controller) {
  // Dropping the receiver in the binder means no real capture stream is ever
  // opened.
  auto audio_controller = std::make_unique<AudioController>(
      base::BindRepeating(
          [](mojo::PendingReceiver<media::mojom::AudioStreamFactory>) {}),
      base::BindLambdaForTesting(
          [this]() -> std::unique_ptr<media::AudioSystem> {
            return std::make_unique<media::AudioSystemImpl>(
                audio_manager_.get());
          }));

  return MakeConversationImplForTesting(session_controller,
                                        std::move(audio_controller));
}

}  // namespace ttc
