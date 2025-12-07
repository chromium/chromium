// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/annotator/annotation_tray.h"
#include "ash/annotator/annotator_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/annotator/test/mock_annotator_client.h"
#include "ash/webui/boca_ui/boca_util.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-data-view.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-forward.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-shared.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/webview_auth_delegate.h"
#include "ash/webui/boca_ui/webview_auth_handler.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/add_students_request.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/create_session_request.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/join_session_request.h"
#include "chromeos/ash/components/boca/session_api/remove_student_request.h"
#include "chromeos/ash/components/boca/session_api/renotify_student_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/session_api/update_session_request.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_service.h"
#include "chromeos/ash/components/boca/student_screen_presenter.h"
#include "chromeos/ash/components/boca/teacher_screen_presenter.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/fake_browser_context_helper_delegate.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "components/account_id/account_id.h"
#include "components/content_settings/core/browser/content_settings_policy_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {
namespace {
constexpr GaiaId::Literal kGaiaId("123");
constexpr char kUserEmail[] = "cat@gmail.com";
constexpr char kWebviewHostName[] = "boca";
constexpr char kTestDefaultUrl[] = "https://test";
constexpr char kTestUrlBase[] = "https://test";
constexpr char kBocaSpotlightViewStudentScreenErrorCodeUmaPath[] =
    "Ash.Boca.Spotlight.ViewStudentScreen.ErrorCode";
constexpr char kBocaSpotlightEndViewStudentScreenErrorCodeUmaPath[] =
    "Ash.Boca.Spotlight.EndViewStudentScreen.ErrorCode";
constexpr char kBocaSpotlightSetViewScreenSessionActiveErrorCodeUmaPath[] =
    "Ash.Boca.Spotlight.SetViewScreenSessionActive.ErrorCode";
constexpr char kBocaGetSessionErrorCodeUmaPath[] =
    "Ash.Boca.GetSession.ErrorCode";
constexpr char kBocaCreateSessionErrorCodeUmaPath[] =
    "Ash.Boca.CreateSession.ErrorCode";
constexpr char kBocaEndSessionErrorCodeUmaPath[] =
    "Ash.Boca.EndSession.ErrorCode";
constexpr char kBocaUpdateSessionErrorCodeUmaPath[] =
    "Ash.Boca.UpdateSession.ErrorCode";
constexpr char kBocaJoinSessionViaAccessCodeErrorCodeUmaPath[] =
    "Ash.Boca.JoinSessionViaAccessCode.ErrorCode";
constexpr char UpdateCaptionErrorCodeUmaPath[] =
    "Ash.Boca.UpdateCaption.ErrorCode";
constexpr char kBocaAddStudentsErrorCodeUmaPath[] =
    "Ash.Boca.AddStudents.ErrorCode";
constexpr char kBocaRemoveStudentErrorCodeUmaPath[] =
    "Ash.Boca.RemoveStudent.ErrorCode";
constexpr char kBocaPresentOwnScreenOutOfSessionResultUmaPath[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenOutOfSession.Result";
constexpr char kBocaPresentOwnScreenOutOfSessionFailureReasonUmaPath[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenOutOfSession.FailureReason";
constexpr char kBocaPresentOwnScreenInSessionResultUmaPath[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenInSession.Result";
constexpr char kBocaPresentOwnScreenInSessionFailureReasonUmaPath[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenInSession.FailureReason";
constexpr char kBocaPresentStudentScreenResultUmaPath[] =
    "Ash.Boca.ScreenShare.PresentStudentScreen.Result";
constexpr char kBocaPresentStudentScreenFailureReasonUmaPath[] =
    "Ash.Boca.ScreenShare.PresentStudentScreen.FailureReason";
constexpr char kStudentDeviceId[] = "student_device_id";
constexpr char kActiveStudentId[] = "active_student_id";
constexpr char kReceiverId[] = "receiver_id";
constexpr char kReceiverName[] = "receiver_name";

mojom::OnTaskConfigPtr GetCommonTestLockOnTaskConfig() {
  std::vector<mojom::ControlledTabPtr> tabs;
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New(1, "google", GURL("http://google.com/"),
                          GURL("http://data/image")),
      /*navigation_type=*/mojom::NavigationType::kOpen));
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New(2, "youtube", GURL("http://youtube.com/"),
                          GURL("http://data/image")),
      /*navigation_type=*/mojom::NavigationType::kBlock));
  return mojom::OnTaskConfig::New(/*is_locked=*/true, /*is_paused=*/true,
                                  std::move(tabs));
}

mojom::OnTaskConfigPtr GetCommonTestUnLockedOnTaskConfig() {
  std::vector<mojom::ControlledTabPtr> tabs;
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New(1, "google", GURL("http://google.com/"),
                          GURL("http://data/image")),
      /*navigation_type=*/mojom::NavigationType::kOpen));
  return mojom::OnTaskConfig::New(/*is_locked=*/false, /*is_paused=*/false,
                                  std::move(tabs));
}

::boca::OnTaskConfig GetCommonTestLockOnTaskConfigProto() {
  ::boca::OnTaskConfig on_task_config;
  auto* active_bundle = on_task_config.mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->set_lock_to_app_home(true);
  auto* content = active_bundle->mutable_content_configs()->Add();
  content->set_url("http://google.com/");
  content->set_favicon_url("http://data/image");
  content->set_title("google");
  content->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);
  auto* content_1 = active_bundle->mutable_content_configs()->Add();
  content_1->set_url("http://youtube.com/");
  content_1->set_favicon_url("http://data/image");
  content_1->set_title("youtube");
  content_1->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType_BLOCK_NAVIGATION);
  return on_task_config;
}

::boca::OnTaskConfig GetCommonTestUnLockOnTaskConfigProto() {
  ::boca::OnTaskConfig on_task_config;
  auto* active_bundle = on_task_config.mutable_active_bundle();
  active_bundle->set_locked(false);
  active_bundle->set_lock_to_app_home(false);
  auto* content = active_bundle->mutable_content_configs()->Add();
  content->set_url("http://google.com/");
  content->set_favicon_url("http://data/image");
  content->set_title("google");
  content->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);
  return on_task_config;
}

mojom::CaptionConfigPtr GetCommonCaptionConfig() {
  return mojom::CaptionConfig::New(true, true, true);
}

::boca::CaptionsConfig GetCommonCaptionConfigProto() {
  ::boca::CaptionsConfig config;
  config.set_captions_enabled(true);
  config.set_translations_enabled(true);
  return config;
}

struct SessionOptions {
  bool captions_enabled = false;
  bool translations_enabled = false;
};

::boca::Session GetCommonActiveSessionProto(
    SessionOptions opts = SessionOptions()) {
  ::boca::Session session;
  session.mutable_duration()->set_seconds(120);
  session.set_session_state(::boca::Session::ACTIVE);
  auto* teacher = session.mutable_teacher();
  teacher->set_gaia_id("123");

  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(opts.captions_enabled);
  caption_config_1->set_translations_enabled(opts.translations_enabled);

  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(false);
  active_bundle->set_lock_to_app_home(false);
  auto* content = active_bundle->mutable_content_configs()->Add();
  content->set_url("http://default.com/");
  content->set_favicon_url("http://data/image");
  content->set_title("default");
  content->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);

  (*session.mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);
  return session;
}

class MockSessionClientImpl : public SessionClientImpl {
 public:
  explicit MockSessionClientImpl(
      std::unique_ptr<google_apis::RequestSender> sender)
      : SessionClientImpl(std::move(sender)) {}
  MOCK_METHOD(void,
              CreateSession,
              (std::unique_ptr<CreateSessionRequest>),
              (override));
  MOCK_METHOD(void,
              GetSession,
              (std::unique_ptr<GetSessionRequest>, bool),
              (override));
  MOCK_METHOD(void,
              UpdateSession,
              (std::unique_ptr<UpdateSessionRequest>),
              (override));
  MOCK_METHOD(void,
              RemoveStudent,
              (std::unique_ptr<RemoveStudentRequest>),
              (override));
  MOCK_METHOD(void,
              AddStudents,
              (std::unique_ptr<AddStudentsRequest>),
              (override));
  MOCK_METHOD(void,
              JoinSession,
              (std::unique_ptr<JoinSessionRequest>),
              (override));
  MOCK_METHOD(void,
              RenotifyStudent,
              (std::unique_ptr<RenotifyStudentRequest>),
              (override));
};

class MockBocaAppClient : public BocaAppClient {
 public:
  MOCK_METHOD(BocaSessionManager*, GetSessionManager, (), (override));
  MOCK_METHOD(void, AddSessionManager, (BocaSessionManager*), (override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(std::string, GetSchoolToolsServerBaseUrl, (), (override));
  MOCK_METHOD(void, OpenFeedbackDialog, (), (override));
  MOCK_METHOD(int, GetAppInstanceCount, (), (override));
};

class MockSessionManager : public BocaSessionManager {
 public:
  explicit MockSessionManager(SessionClientImpl* session_client_impl)
      : BocaSessionManager(session_client_impl,
                           /*pref_service=*/nullptr,
                           AccountId::FromUserEmail(kUserEmail),
                           /*=is_producer*/ false) {}
  MOCK_METHOD(void,
              NotifyLocalCaptionEvents,
              (::boca::CaptionsConfig config),
              (override));
  MOCK_METHOD(void,
              UpdateCurrentSession,
              (std::unique_ptr<::boca::Session>, bool),
              (override));
  MOCK_METHOD((::boca::Session*), GetCurrentSession, (), (override));
  MOCK_METHOD(void, LoadCurrentSession, (bool), (override));
  MOCK_METHOD(void, OnAppWindowOpened, (), (override));
  MOCK_METHOD(void, NotifyAppReload, (), (override));
  MOCK_METHOD(void,
              NotifySessionCaptionProducerEvents,
              (const ::boca::CaptionsConfig&),
              (override));
  MOCK_METHOD(bool, disabled_on_non_managed_network, (), (override));
  MOCK_METHOD(StudentScreenPresenter*,
              GetStudentScreenPresenter,
              (),
              (override));
  MOCK_METHOD(TeacherScreenPresenter*,
              GetTeacherScreenPresenter,
              (),
              (override));
  MOCK_METHOD(std::optional<std::string>,
              GetStudentActiveDeviceId,
              (std::string_view),
              (override));
  MOCK_METHOD(void, EndSpotlightSession, (base::OnceClosure), (override));
  MOCK_METHOD(void, CleanupPresenters, (), (override));
  ~MockSessionManager() override = default;
};

class MockSpotlightService : public SpotlightService {
 public:
  explicit MockSpotlightService(
      std::unique_ptr<google_apis::RequestSender> sender)
      : SpotlightService(std::move(sender)) {}
  MOCK_METHOD(void,
              ViewScreen,
              (std::string, std::string, ViewScreenRequestCallback),
              (override));
  MOCK_METHOD(void,
              UpdateViewScreenState,
              (std::string,
               ::boca::ViewScreenConfig::ViewScreenState,
               std::string,
               ViewScreenRequestCallback),
              (override));
};

class MockWebviewAuthHandler : public WebviewAuthHandler {
 public:
  MockWebviewAuthHandler(content::BrowserContext* context,
                         const std::string& webview_host_name)
      : WebviewAuthHandler(std::make_unique<WebviewAuthDelegate>(),
                           context,
                           webview_host_name) {}
  MockWebviewAuthHandler(const MockWebviewAuthHandler&) = delete;
  MockWebviewAuthHandler& operator=(const WebviewAuthHandler&) = delete;
  ~MockWebviewAuthHandler() override {}

  MOCK_METHOD1(AuthenticateWebview, void(AuthenticateWebviewCallback));
};

class MockStudentScreenPresenter : public StudentScreenPresenter {
 public:
  MockStudentScreenPresenter() = default;
  ~MockStudentScreenPresenter() override = default;

  MOCK_METHOD(void,
              Start,
              (std::string_view,
               const ::boca::UserIdentity&,
               std::string_view,
               base::OnceCallback<void(bool)>,
               base::OnceClosure),
              (override));

  MOCK_METHOD(void, CheckConnection, (), (override));

  MOCK_METHOD(void, Stop, (base::OnceCallback<void(bool)>), (override));

  MOCK_METHOD(bool,
              IsPresenting,
              (std::optional<std::string_view>),
              (override));
};

class MockTeacherScreenPresenter : public TeacherScreenPresenter {
 public:
  MockTeacherScreenPresenter() = default;
  ~MockTeacherScreenPresenter() override = default;

  MOCK_METHOD(void,
              Start,
              (std::string_view,
               std::string_view,
               ::boca::UserIdentity,
               bool,
               base::OnceCallback<void(bool)>,
               base::OnceClosure),
              (override));

  MOCK_METHOD(void, Stop, (base::OnceCallback<void(bool)>), (override));

  MOCK_METHOD(bool, IsPresenting, (), (override));
};

class FakePage : public mojom::Page {
 public:
  using ActivityInterceptorCallback =
      base::OnceCallback<void(std::vector<mojom::IdentifiedActivityPtr>)>;
  using SessionConfigInterceptorCallback =
      base::OnceCallback<void(mojom::ConfigResultPtr)>;
  explicit FakePage(mojo::PendingReceiver<mojom::Page> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  FakePage(const FakePage&) = delete;
  FakePage& operator=(const FakePage&) = delete;

  ~FakePage() override = default;

  void SetActivityInterceptorCallback(
      ActivityInterceptorCallback student_activity_updated_cb) {
    student_activity_updated_cb_ = std::move(student_activity_updated_cb);
  }

  void SetSessionConfigInterceptorCallback(
      SessionConfigInterceptorCallback session_config_updated_cb) {
    session_config_updated_cb_ = std::move(session_config_updated_cb);
  }

  void SetLocalCaptionDisabledInterceptorCallback(
      base::OnceClosure local_caption_disabled_cb) {
    local_caption_disabled_cb_ = std::move(local_caption_disabled_cb);
  }

  void OnSpeechRecognitionInstallStateUpdated(
      mojom::SpeechRecognitionInstallState state) override {}

  void SetSessionCaptionDisabledInterceptorCallback(
      base::OnceCallback<void(bool)> session_caption_disabled_cb) {
    session_caption_disabled_cb_ = std::move(session_caption_disabled_cb);
  }

  void SetPresentStudentScreenEndedInterceptorCallback(
      base::OnceClosure present_student_screen_ended_cb) {
    present_student_screen_ended_cb_ =
        std::move(present_student_screen_ended_cb);
  }

  void SetPresentOwnScreenEndedInterceptorCallback(
      base::OnceClosure present_own_screen_ended_cb) {
    present_own_screen_ended_cb_ = std::move(present_own_screen_ended_cb);
  }

  void OnSpotlightCrdSessionStatusUpdated(
      mojom::CrdConnectionState state) override {}

 private:
  // mojom::Page:
  void OnStudentActivityUpdated(
      std::vector<mojom::IdentifiedActivityPtr> activities) override {
    if (student_activity_updated_cb_) {
      std::move(student_activity_updated_cb_).Run(std::move(activities));
    }
  }
  void OnSessionConfigUpdated(mojom::ConfigResultPtr config) override {
    if (session_config_updated_cb_) {
      std::move(session_config_updated_cb_).Run(std::move(config));
    }
  }
  void OnActiveNetworkStateChanged(
      std::vector<mojom::NetworkInfoPtr> active_networks) override {}
  void OnLocalCaptionDisabled() override {
    if (local_caption_disabled_cb_) {
      std::move(local_caption_disabled_cb_).Run();
    }
  }
  void OnSessionCaptionDisabled(bool is_error) override {
    if (session_caption_disabled_cb_) {
      std::move(session_caption_disabled_cb_).Run(is_error);
    }
  }
  void OnFrameDataReceived(const SkBitmap& frame_data) override {}

  void OnPresentStudentScreenEnded() override {
    if (present_student_screen_ended_cb_) {
      std::move(present_student_screen_ended_cb_).Run();
    }
  }

  void OnPresentOwnScreenEnded() override {
    if (present_own_screen_ended_cb_) {
      std::move(present_own_screen_ended_cb_).Run();
    }
  }

  ActivityInterceptorCallback student_activity_updated_cb_;
  SessionConfigInterceptorCallback session_config_updated_cb_;
  base::OnceClosure local_caption_disabled_cb_;
  base::OnceCallback<void(bool)> session_caption_disabled_cb_;
  base::OnceClosure present_student_screen_ended_cb_;
  base::OnceClosure present_own_screen_ended_cb_;

  const mojo::Receiver<mojom::Page> receiver_;
};

class BocaAppPageHandlerTest : public testing::Test {
 public:
  BocaAppPageHandlerTest() = default;
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kBoca, ash::features::kBocaScreenSharingStudent,
         ash::features::kBocaScreenSharingTeacher},
        // TODO:crbug.com/424867979 - Re-enable feature flag after adding unit
        // tests.
        /*disabled_features=*/{ash::features::kBocaSpotlightRobotRequester,
                               ash::features::kAnnotatorMode});
    // Set up UserManager related modules.
    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    ash::boca_util::RegisterPrefs(local_state_.registry());
    local_state_.SetDict(ash::prefs::kClassManagementToolsKioskReceiverCodes,
                         base::Value::Dict().Set(kReceiverId, kReceiverName));
    content_settings::PolicyProvider::RegisterProfilePrefs(
        pref_service_.registry());
    fake_user_manager_.Reset(
        std::make_unique<user_manager::FakeUserManager>(&local_state_));

    auto account_id = AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaId);
    auto browser_context_helper_delegate =
        std::make_unique<ash::FakeBrowserContextHelperDelegate>();
    auto* browser_context_helper_delegate_ptr =
        browser_context_helper_delegate.get();
    browser_context_helper_ = std::make_unique<ash::BrowserContextHelper>(
        std::move(browser_context_helper_delegate));

    // Set up global BocaAppClient's mock.
    boca_app_client_ = std::make_unique<NiceMock<MockBocaAppClient>>();
    EXPECT_CALL(*boca_app_client_, AddSessionManager(_)).Times(1);
    ON_CALL(*boca_app_client_, GetIdentityManager())
        .WillByDefault(Return(nullptr));
    ON_CALL(*boca_app_client_, GetSchoolToolsServerBaseUrl())
        .WillByDefault(Return(kTestDefaultUrl));

    // Sign in a test user.
    fake_user_manager_->AddGaiaUser(account_id,
                                    user_manager::UserType::kRegular);
    fake_user_manager_->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
    browser_context_ =
        browser_context_helper_delegate_ptr->CreateBrowserContext(
            browser_context_helper_delegate_ptr->GetUserDataDir()->AppendASCII(
                "test-browser-context"),
            /*is_off_the_record=*/false);
    ash::AnnotatedAccountId::Set(browser_context_, account_id);

    // Create BocaSessionManager mock.
    EXPECT_CALL(*session_client_impl(),
                GetSession(_, /*can_skip_duplicate_request=*/true))
        .Times(1);
    session_manager_ =
        std::make_unique<NiceMock<MockSessionManager>>(&session_client_impl_);

    // Create the WebContents for the BrowserContext.
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser_context_));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());
  }

  void TearDown() override {
    VerifyEndSession();
    browser_context_ = nullptr;
    boca_app_handler_.reset();
    web_ui_.reset();
    web_contents_.reset();
    session_manager_.reset();
    boca_app_client_.reset();
    browser_context_helper_.reset();
    fake_user_manager_.Reset();
  }

 protected:
  void CreateBocaAppHandler(bool is_producer) {
    is_producer_ = is_producer;
    boca_app_handler_ =
        CreateNewBocaAppHandler(is_producer, &remote_, &fake_page_);
  }

  std::unique_ptr<BocaAppHandler> CreateNewBocaAppHandler(
      bool is_producer,
      mojo::Remote<mojom::PageHandler>* remote,
      std::unique_ptr<FakePage>* fake_page) {
    mojo::PendingReceiver<mojom::Page> page_pending_receiver;
    remote->reset();
    // `BocaAppClient::GetSessionManager` should be called exactly once on
    // construction.
    EXPECT_CALL(*boca_app_client(), GetSessionManager)
        .WillOnce(Return(session_manager()));
    auto boca_app_handler = std::make_unique<BocaAppHandler>(
        remote->BindNewPipeAndPassReceiver(),
        // TODO(crbug.com/359929870): Setting nullptr for other dependencies for
        // now. Adding test case for classroom and tab info.
        page_pending_receiver.InitWithNewPipeAndPassRemote(), web_ui_.get(),
        std::make_unique<MockWebviewAuthHandler>(browser_context_,
                                                 kWebviewHostName),
        /*classroom_client_impl=*/nullptr,
        /*content_settings_handler=*/nullptr,
        /*system_web_app_manager=*/nullptr, &session_client_impl_, is_producer);
    *fake_page = std::make_unique<FakePage>(std::move(page_pending_receiver));
    boca_app_handler->SetSpotlightService(&spotlight_service_);
    // Explicitly set pref
    boca_app_handler->SetPrefForTesting(&local_state_);
    return boca_app_handler;
  }

  void PrepareGetSession(::boca::Session* current_session,
                         const ::boca::Session& response_session) {
    EXPECT_CALL(*session_manager(),
                UpdateCurrentSession(_, /*dispatch_event=*/true))
        .WillRepeatedly(
            [current_session](std::unique_ptr<::boca::Session> session, bool) {
              *current_session = *session;
            });
    EXPECT_CALL(*session_client_impl(),
                GetSession(_, /*can_skip_duplicate_request=*/false))
        .WillOnce(
            [&response_session](std::unique_ptr<GetSessionRequest> request,
                                bool can_skip_duplicate_request) {
              auto result = std::make_unique<::boca::Session>(response_session);
              request->callback().Run(std::move(result));
            });
    EXPECT_CALL(*session_manager(), GetCurrentSession())
        .WillRepeatedly(Return(current_session));
    EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
        .WillOnce(Return(false));
  }

  void TestUserPref(mojom::BocaValidPref pref, base::Value value) {
    base::test::TestFuture<void> set_pref_future;
    boca_app_handler_.get()->SetUserPref(pref, value.Clone(),
                                         set_pref_future.GetCallback());
    set_pref_future.Get();

    base::test::TestFuture<base::Value> get_pref_future;
    boca_app_handler_.get()->GetUserPref(pref, get_pref_future.GetCallback());

    EXPECT_EQ(get_pref_future.Get(), value);
  }

  void VerifyEndSession() {
    if (!is_producer_) {
      return;
    }
    EXPECT_CALL(*boca_app_client(), GetAppInstanceCount).WillOnce(Return(1));
    EXPECT_CALL(*session_manager(), GetCurrentSession())
        .WillOnce(Return(&session));
    EXPECT_CALL(*session_manager(),
                UpdateCurrentSession(_, /*dispatch_event=*/true))
        .Times(1);

    // Page handler callback.
    base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                          google_apis::ApiErrorCode>>
        future;
    ::boca::UserIdentity teacher;
    teacher.set_gaia_id(kGaiaId.ToString());
    UpdateSessionRequest request(nullptr, kTestUrlBase, teacher,
                                 session.session_id(), future.GetCallback());
    EXPECT_CALL(*session_client_impl(), UpdateSession(_))
        .WillOnce(WithArg<0>(
            // Unique pointer have ownership issue, have to do manual deep copy
            // here instead of using SaveArg.
            [&](auto request) {
              ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
              ASSERT_EQ(::boca::Session::PAST, *request->session_state());
              request->callback().Run(std::make_unique<::boca::Session>());
            }));
  }

  MockSessionClientImpl* session_client_impl() { return &session_client_impl_; }
  MockBocaAppClient* boca_app_client() { return boca_app_client_.get(); }
  MockSessionManager* session_manager() { return session_manager_.get(); }
  BocaAppHandler* boca_app_handler() { return boca_app_handler_.get(); }
  MockSpotlightService* spotlight_service() { return &spotlight_service_; }
  MockWebviewAuthHandler* webview_auth_handler() {
    return static_cast<MockWebviewAuthHandler*>(
        boca_app_handler_.get()->GetWebviewAuthHandlerForTesting());
  }
  FakePage* fake_page() { return fake_page_.get(); }
  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }

  void SetSessionCaptionInitializer(bool success) {
    session_manager()->SetSessionCaptionInitializer(base::BindLambdaForTesting(
        [success](base::OnceCallback<void(bool)> success_cb) {
          std::move(success_cb).Run(success);
        }));
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  bool is_producer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestingPrefServiceSimple local_state_;
  ::boca::Session session = GetCommonActiveSessionProto();
  session_manager::SessionManager device_session_manager_{
      std::make_unique<session_manager::FakeSessionManagerDelegate>()};

  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  std::unique_ptr<ash::BrowserContextHelper> browser_context_helper_;
  // Among all BocaAppHandler dependencies,BocaAppClient should construct early
  // and destruct last.
  std::unique_ptr<NiceMock<MockBocaAppClient>> boca_app_client_;

  StrictMock<MockSessionClientImpl> session_client_impl_{nullptr};
  std::unique_ptr<NiceMock<MockSessionManager>> session_manager_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  mojo::Remote<mojom::PageHandler> remote_;
  std::unique_ptr<FakePage> fake_page_;
  std::unique_ptr<BocaAppHandler> boca_app_handler_;
  StrictMock<MockSpotlightService> spotlight_service_{nullptr};
  raw_ptr<content::BrowserContext> browser_context_;
};

class BocaAppPageHandlerProducerTest : public BocaAppPageHandlerTest {
 public:
  void SetUp() override {
    BocaAppPageHandlerTest::SetUp();
    CreateBocaAppHandler(/*is_producer=*/true);
  }

  void TearDown() override {
    EXPECT_CALL(*session_manager(), CleanupPresenters).Times(1);
    BocaAppPageHandlerTest::TearDown();
  }
};

class BocaAppPageHandlerConsumerTest : public BocaAppPageHandlerTest {
 public:
  void SetUp() override {
    BocaAppPageHandlerTest::SetUp();
    CreateBocaAppHandler(/*is_producer=*/false);
  }
};

TEST_F(BocaAppPageHandlerProducerTest, CreateSessionWithFullInput) {
  auto session_duration = base::Minutes(2);

  std::vector<mojom::IdentityPtr> students;
  students.push_back(
      mojom::Identity::New("1", "a", "a@gmail.com", GURL("cdn://s1")));
  students.push_back(
      mojom::Identity::New("2", "b", "b@gmail.com", GURL("cdn://s2")));

  const auto config = mojom::Config::New(
      session_duration, std::nullopt, nullptr, std::move(students),
      std::vector<mojom::IdentityPtr>{}, GetCommonTestLockOnTaskConfig(),
      GetCommonCaptionConfig(), "");
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::CreateSessionError>> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  CreateSessionRequest request(
      nullptr, kTestUrlBase, teacher, config->session_duration,
      ::boca::Session::SessionState::Session_SessionState_ACTIVE,
      future.GetCallback());
  EXPECT_CALL(*session_client_impl(), CreateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
            ASSERT_EQ(session_duration, request->duration());
            ASSERT_EQ(
                ::boca::Session::SessionState::Session_SessionState_ACTIVE,
                request->session_state());

            // Optional attribute.
            ASSERT_EQ(1, request->roster()->student_groups().size());
            ASSERT_EQ(2,
                      request->roster()->student_groups()[0].students().size());
            EXPECT_EQ(
                "1",
                request->roster()->student_groups()[0].students()[0].gaia_id());
            EXPECT_EQ("a", request->roster()
                               ->student_groups()[0]
                               .students()[0]
                               .full_name());
            EXPECT_EQ(
                "a@gmail.com",
                request->roster()->student_groups()[0].students()[0].email());
            EXPECT_EQ("cdn://s1", request->roster()
                                      ->student_groups()[0]
                                      .students()[0]
                                      .photo_url());

            EXPECT_EQ(
                "2",
                request->roster()->student_groups()[0].students()[1].gaia_id());
            EXPECT_EQ("b", request->roster()
                               ->student_groups()[0]
                               .students()[1]
                               .full_name());
            EXPECT_EQ(
                "b@gmail.com",
                request->roster()->student_groups()[0].students()[1].email());
            EXPECT_EQ("cdn://s2", request->roster()
                                      ->student_groups()[0]
                                      .students()[1]
                                      .photo_url());
            ASSERT_TRUE(request->on_task_config());
            EXPECT_TRUE(request->on_task_config()->active_bundle().locked());
            EXPECT_TRUE(
                request->on_task_config()->active_bundle().lock_to_app_home());
            ASSERT_EQ(2, request->on_task_config()
                             ->active_bundle()
                             .content_configs()
                             .size());
            EXPECT_EQ("google", request->on_task_config()
                                    ->active_bundle()
                                    .content_configs()[0]
                                    .title());
            EXPECT_EQ("http://google.com/", request->on_task_config()
                                                ->active_bundle()
                                                .content_configs()[0]
                                                .url());
            EXPECT_EQ("http://data/image", request->on_task_config()
                                               ->active_bundle()
                                               .content_configs()[0]
                                               .favicon_url());
            EXPECT_EQ(
                ::boca::LockedNavigationOptions::NavigationType::
                    LockedNavigationOptions_NavigationType_OPEN_NAVIGATION,
                request->on_task_config()
                    ->active_bundle()
                    .content_configs()[0]
                    .locked_navigation_options()
                    .navigation_type());

            EXPECT_EQ("youtube", request->on_task_config()
                                     ->active_bundle()
                                     .content_configs()[1]
                                     .title());
            EXPECT_EQ("http://youtube.com/", request->on_task_config()
                                                 ->active_bundle()
                                                 .content_configs()[1]
                                                 .url());
            EXPECT_EQ("http://data/image", request->on_task_config()
                                               ->active_bundle()
                                               .content_configs()[1]
                                               .favicon_url());
            EXPECT_EQ(
                ::boca::LockedNavigationOptions::NavigationType::
                    LockedNavigationOptions_NavigationType_BLOCK_NAVIGATION,
                request->on_task_config()
                    ->active_bundle()
                    .content_configs()[1]
                    .locked_navigation_options()
                    .navigation_type());

            ASSERT_TRUE(request->captions_config());
            EXPECT_TRUE(request->captions_config()->captions_enabled());
            EXPECT_TRUE(request->captions_config()->translations_enabled());
            request->callback().Run(std::make_unique<::boca::Session>());
          }));

  // Verify local events dispatched
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));

  boca_app_handler()->CreateSession(config->Clone(), future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, CreateSessionWithCritialInputOnly) {
  auto session_duration = base::Minutes(2);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::CreateSessionError>> future_1;

  const auto config = mojom::Config::New(
      session_duration, std::nullopt, nullptr,
      std::vector<mojom::IdentityPtr>{}, std::vector<mojom::IdentityPtr>{},
      mojom::OnTaskConfigPtr(nullptr), mojom::CaptionConfigPtr(nullptr), "");

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  CreateSessionRequest request(
      nullptr, kTestUrlBase, teacher, config->session_duration,
      ::boca::Session::SessionState::Session_SessionState_ACTIVE,
      future.GetCallback());
  EXPECT_CALL(*session_client_impl(), CreateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
            ASSERT_EQ(session_duration, request->duration());
            ASSERT_EQ(
                ::boca::Session::SessionState::Session_SessionState_ACTIVE,
                request->session_state());

            ASSERT_FALSE(request->captions_config());
            ASSERT_FALSE(request->on_task_config());
            ASSERT_TRUE(request->roster());
            request->callback().Run(std::make_unique<::boca::Session>());
          }));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);

  // Verify local events not dispatched
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(0);
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));

  boca_app_handler()->CreateSession(config.Clone(), future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, CreateSessionFailedWithHttpError) {
  base::HistogramTester histogram_tester;
  auto session_duration = base::Minutes(2);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::CreateSessionError>> future_1;

  const auto config = mojom::Config::New(
      session_duration, std::nullopt, nullptr,
      std::vector<mojom::IdentityPtr>{}, std::vector<mojom::IdentityPtr>{},
      mojom::OnTaskConfigPtr(nullptr), mojom::CaptionConfigPtr(nullptr), "");

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  CreateSessionRequest request(
      nullptr, kTestUrlBase, teacher, config->session_duration,
      ::boca::Session::SessionState::Session_SessionState_ACTIVE,
      future.GetCallback());
  EXPECT_CALL(*session_client_impl(), CreateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
            ASSERT_EQ(session_duration, request->duration());
            ASSERT_EQ(
                ::boca::Session::SessionState::Session_SessionState_ACTIVE,
                request->session_state());

            ASSERT_FALSE(request->captions_config());
            ASSERT_FALSE(request->on_task_config());
            ASSERT_TRUE(request->roster());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          }));

  // Verify local events not dispatched
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(0);
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));

  boca_app_handler()->CreateSession(config.Clone(), future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  histogram_tester.ExpectTotalCount(kBocaCreateSessionErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaCreateSessionErrorCodeUmaPath,
                                     google_apis::ApiErrorCode::HTTP_FORBIDDEN,
                                     1);
}

TEST_F(BocaAppPageHandlerProducerTest, CreateSessionFailedOnNonManagedNetwork) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::CreateSessionError>> future_1;

  const auto config = mojom::Config::New(
      base::Minutes(2), std::nullopt, nullptr,
      std::vector<mojom::IdentityPtr>{}, std::vector<mojom::IdentityPtr>{},
      mojom::OnTaskConfigPtr(nullptr), GetCommonCaptionConfig(), "");

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  CreateSessionRequest request(
      nullptr, kTestUrlBase, teacher, config->session_duration,
      ::boca::Session::SessionState::Session_SessionState_ACTIVE,
      future.GetCallback());
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(true));
  EXPECT_CALL(*session_client_impl(), CreateSession(_)).Times(0);

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(0);

  // Verify local events dispatched
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);

  boca_app_handler()->CreateSession(config.Clone(), future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  ASSERT_TRUE(future_1.Get().has_value());
  EXPECT_EQ(mojom::CreateSessionError::kNetworkRestriction,
            future_1.Get().value());
}

TEST_F(BocaAppPageHandlerConsumerTest, GetSessionWithFullInputTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/false))
      .WillOnce(WithArg<0>([&](auto request) {
        auto session = std::make_unique<::boca::Session>();
        auto* start_time = session->mutable_start_time();
        start_time->set_seconds(1111111);
        start_time->set_nanos(22000000);

        session->mutable_duration()->set_seconds(120);
        session->set_session_state(::boca::Session::ACTIVE);
        auto* teacher = session->mutable_teacher();
        teacher->set_email("teacher@email.com");
        teacher->set_full_name("teacher");
        teacher->set_gaia_id("000");
        teacher->set_photo_url("cdn://s");

        auto* access_code = session->mutable_join_code();
        access_code->set_code("testCode");

        auto* student_groups_1 =
            session->mutable_roster()->mutable_student_groups()->Add();
        student_groups_1->set_title(kMainStudentGroupName);
        student_groups_1->set_group_source(::boca::StudentGroup::CLASSROOM);
        auto* student = student_groups_1->mutable_students()->Add();
        student->set_email("dog@email.com");
        student->set_full_name("dog");
        student->set_gaia_id("111");
        student->set_photo_url("cdn://s1");

        auto* student_groups_2 =
            session->mutable_roster()->mutable_student_groups()->Add();
        student_groups_2->set_title("accessCode");
        student_groups_2->set_group_source(::boca::StudentGroup::JOIN_CODE);
        auto* student_2 = student_groups_2->mutable_students()->Add();
        student_2->set_email("dog1@email.com");
        student_2->set_full_name("dog1");
        student_2->set_gaia_id("222");
        student_2->set_photo_url("cdn://s2");

        ::boca::SessionConfig session_config;
        auto* caption_config_1 = session_config.mutable_captions_config();

        caption_config_1->set_captions_enabled(true);
        caption_config_1->set_translations_enabled(true);

        auto* active_bundle =
            session_config.mutable_on_task_config()->mutable_active_bundle();
        active_bundle->set_locked(true);
        active_bundle->set_lock_to_app_home(true);
        auto* content = active_bundle->mutable_content_configs()->Add();
        content->set_url("http://google.com/");
        content->set_favicon_url("http://data/image");
        content->set_title("google");
        content->mutable_locked_navigation_options()->set_navigation_type(
            ::boca::LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);

        (*session->mutable_student_group_configs())[kMainStudentGroupName] =
            std::move(session_config);

        auto* student_statuses = session->mutable_student_statuses();
        std::map<std::string, ::boca::StudentStatus> activities;
        ::boca::StudentStatus status_1;
        status_1.set_state(::boca::StudentStatus::ACTIVE);
        ::boca::StudentDevice device_1;
        auto* activity_1 = device_1.mutable_activity();
        activity_1->mutable_active_tab()->set_title("google");
        ::boca::StudentDevice device_11;
        device_1.set_state(::boca::StudentDevice::INACTIVE);

        (*status_1.mutable_devices())["device1"] = std::move(device_1);
        (*student_statuses)["111"] = std::move(status_1);

        request->callback().Run(std::move(session));
      }));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(NotNull(), /*dispatch_event=*/true))
      .Times(1);

  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));
  boca_app_handler()->GetSession(future_1.GetCallback());

  auto result0 = std::move(future_1.Take()->get_session());
  auto result = std::move(result0->config);
  EXPECT_EQ(120, result->session_duration.InSeconds());
  EXPECT_EQ(1111111.022,
            result->session_start_time->InSecondsFSinceUnixEpoch());
  EXPECT_EQ("teacher", result->teacher->name);
  EXPECT_EQ("teacher@email.com", result->teacher->email);
  EXPECT_EQ("000", result->teacher->id);
  EXPECT_EQ("cdn://s", result->teacher->photo_url->spec());

  EXPECT_EQ("testCode", result->access_code);
  EXPECT_EQ(true, result->caption_config->session_caption_enabled);
  EXPECT_EQ(true, result->caption_config->session_translation_enabled);

  ASSERT_EQ(1u, result->students.size());

  EXPECT_EQ("dog", result->students[0]->name);
  EXPECT_EQ("111", result->students[0]->id);
  EXPECT_EQ("dog@email.com", result->students[0]->email);
  EXPECT_EQ("cdn://s1", result->students[0]->photo_url->spec());

  ASSERT_EQ(1u, result->students_join_via_code.size());

  EXPECT_EQ("dog1", result->students_join_via_code[0]->name);
  EXPECT_EQ("222", result->students_join_via_code[0]->id);
  EXPECT_EQ("dog1@email.com", result->students_join_via_code[0]->email);
  EXPECT_EQ("cdn://s2", result->students_join_via_code[0]->photo_url->spec());

  ASSERT_EQ(1u, result->on_task_config->tabs.size());
  ASSERT_TRUE(result->on_task_config->is_locked);
  ASSERT_TRUE(result->on_task_config->is_paused);
  EXPECT_EQ(mojom::NavigationType::kOpen,
            result->on_task_config->tabs[0]->navigation_type);
  EXPECT_EQ("http://google.com/",
            result->on_task_config->tabs[0]->tab->url.spec());
  EXPECT_EQ("google", result->on_task_config->tabs[0]->tab->title);
  EXPECT_EQ("http://data/image", result->on_task_config->tabs[0]->tab->favicon);

  auto activities = std::move(result0->activities);
  EXPECT_EQ(1u, activities.size());
  EXPECT_FALSE(activities[0]->activity->is_active);
  EXPECT_EQ("google", activities[0]->activity->active_tab);
}

TEST_F(BocaAppPageHandlerProducerTest, GetSessionWithPartialInputTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;
  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/false))
      .WillOnce(WithArg<0>([&](auto request) {
        auto session = std::make_unique<::boca::Session>();
        session->mutable_duration()->set_seconds(120);
        session->set_session_state(::boca::Session::ACTIVE);
        request->callback().Run(std::move(session));
      }));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(NotNull(), /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));
  boca_app_handler()->GetSession(future_1.GetCallback());

  auto result = std::move(future_1.Take()->get_session()->config);
  EXPECT_EQ(120, result->session_duration.InSeconds());
}

TEST_F(BocaAppPageHandlerProducerTest, GetSessionWithHTTPError) {
  base::HistogramTester histogram_tester;
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/false))
      .WillOnce(WithArg<0>([&](auto request) {
        request->callback().Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_BAD_REQUEST));
      }));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(0);
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));
  boca_app_handler()->GetSession(future_1.GetCallback());
  auto result = future_1.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(mojom::GetSessionError::kHTTPError, result->get_error());
  histogram_tester.ExpectTotalCount(kBocaGetSessionErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaGetSessionErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_BAD_REQUEST, 1);
}

TEST_F(BocaAppPageHandlerProducerTest, GetSessionWithNullPtrInputTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/false))
      .WillOnce(WithArg<0>(
          [&](auto request) { request->callback().Run(base::ok(nullptr)); }));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(IsNull(), /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));
  boca_app_handler()->GetSession(future_1.GetCallback());

  auto result = future_1.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(mojom::GetSessionError::kEmpty, result->get_error());
}

TEST_F(BocaAppPageHandlerProducerTest, GetSessionWithNonActiveSessionTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;
  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/false))
      .WillOnce(WithArg<0>([&](auto request) {
        request->callback().Run(std::make_unique<::boca::Session>());
      }));

  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(IsNull(), /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));
  boca_app_handler()->GetSession(future_1.GetCallback());
  auto result = future_1.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(mojom::GetSessionError::kEmpty, result->get_error());
}

TEST_F(BocaAppPageHandlerProducerTest,
       GetSessionWithEmptySessionConfigShouldNotCrashTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/false))
      .WillOnce(WithArg<0>([&](auto request) {
        auto session = std::make_unique<::boca::Session>();
        session->set_session_state(::boca::Session::ACTIVE);
        request->callback().Run(std::move(session));
      }));
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(NotNull(), /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));
  boca_app_handler()->GetSession(future_1.GetCallback());
  auto result = future_1.Take();
  ASSERT_FALSE(result->is_error());
}

TEST_F(BocaAppPageHandlerProducerTest,
       GetSessionWithNonManagedNetworkShouldReturnEmpty) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kTestUrlBase, false, kGaiaId,
                            future.GetCallback());
  EXPECT_CALL(*session_client_impl(),
              GetSession(_, /*can_skip_duplicate_request=*/false))
      .Times(0);
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(testing::IsNull(), /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(true));
  boca_app_handler()->GetSession(future_1.GetCallback());
  auto result = future_1.Take();
  ASSERT_TRUE(result->is_error());
}

TEST_F(BocaAppPageHandlerProducerTest, EndSessionSucceed) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->mutable_duration()->set_seconds(120);
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  UpdateSessionRequest request(nullptr, kTestUrlBase, teacher, session_id,
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
            ASSERT_EQ(::boca::Session::PAST, *request->session_state());
            request->callback().Run(std::make_unique<::boca::Session>());
          }));

  boca_app_handler()->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, EndSessionWithHTTPFailure) {
  base::HistogramTester histogram_tester;
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->mutable_duration()->set_seconds(120);
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  UpdateSessionRequest request(nullptr, kTestUrlBase, teacher, session_id,
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
            ASSERT_EQ(::boca::Session::PAST, *request->session_state());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          }));

  boca_app_handler()->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  histogram_tester.ExpectTotalCount(kBocaEndSessionErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaEndSessionErrorCodeUmaPath,
                                     google_apis::ApiErrorCode::HTTP_FORBIDDEN,
                                     1);
}

TEST_F(BocaAppPageHandlerProducerTest, EndSessionWithEmptyResponse) {
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerProducerTest, EndSessionWithNonActiveResponse) {
  ::boca::Session session;

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerProducerTest, ExtendSessionDurationSucceed) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->mutable_duration()->set_seconds(120);
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(session.get()));
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId.ToString());
  UpdateSessionRequest request(nullptr, kTestUrlBase, teacher, session_id,
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>([&](auto request) {
        ASSERT_EQ(kGaiaId.ToString(), request->teacher().gaia_id());
        ASSERT_EQ(base::Seconds(150), *request->duration());
        request->callback().Run(std::make_unique<::boca::Session>());
      }));

  boca_app_handler()->ExtendSessionDuration(base::Seconds(30),
                                            future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateOnTaskConfigSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            EXPECT_EQ(session.teacher().gaia_id(),
                      request->teacher().gaia_id());
            ASSERT_EQ(GetCommonTestLockOnTaskConfigProto().SerializeAsString(),
                      request->on_task_config()->SerializeAsString());
            // Use latest caption cofig value from session.
            ASSERT_EQ(GetCommonActiveSessionProto()
                          .student_group_configs()
                          .at(kMainStudentGroupName)
                          .captions_config()
                          .SerializeAsString(),
                      request->captions_config()->SerializeAsString());
            request->callback().Run(std::make_unique<::boca::Session>(
                GetCommonActiveSessionProto()));
          }));
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateOnTaskConfigWithEmptySession) {
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateOnTaskConfigWithNonActiveSession) {
  ::boca::Session non_active_session;

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&non_active_session));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateOnTaskConfigWithHTTPFailure) {
  base::HistogramTester histogram_tester;
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(session.teacher().gaia_id(),
                      request->teacher().gaia_id());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          }));

  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kHTTPError, future_1.Get().value());
  histogram_tester.ExpectTotalCount(kBocaUpdateSessionErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaUpdateSessionErrorCodeUmaPath,
                                     google_apis::ApiErrorCode::HTTP_FORBIDDEN,
                                     1);
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateCaptionWithEmptySession) {
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateCaptionWithNonActiveSession) {
  ::boca::Session session;
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateCaptionConfigSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents(_))
      .Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(session.teacher().gaia_id(),
                      request->teacher().gaia_id());
            ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                      request->captions_config()->SerializeAsString());
            // Use latest on task cofig value from session.
            ASSERT_EQ(GetCommonActiveSessionProto()
                          .student_group_configs()
                          .at(kMainStudentGroupName)
                          .on_task_config()
                          .SerializeAsString(),
                      request->on_task_config()->SerializeAsString());
            request->callback().Run(std::make_unique<::boca::Session>(
                GetCommonActiveSessionProto()));
          }));

  SetSessionCaptionInitializer(/*success=*/true);
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());

  // Called on destruction to disable session captions.
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents(_))
      .Times(1);
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateCaptionInitFailed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(0);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_)).Times(0);

  SetSessionCaptionInitializer(/*success=*/false);
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  EXPECT_EQ(future_1.Get().value(),
            mojom::UpdateSessionError::kPreconditionFailed);
}

TEST_F(BocaAppPageHandlerProducerTest,
       UpdateCaptionConfigWithLocalConfigOnlyShouldNotSendServerRequest) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_)).Times(0);

  boca_app_handler()->UpdateCaptionConfig(
      mojom::CaptionConfig::New(/*=session_caption_enabled*/ false,
                                /*local_caption_enabled*/ true,
                                /*=session_translation_enabled*/ false),
      future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateCaptionWithHTTPFailure) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(session.teacher().gaia_id(),
                      request->teacher().gaia_id());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          }));

  SetSessionCaptionInitializer(/*success=*/true);
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kHTTPError, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerProducerTest,
       UpdateCaptionOnTaskSessionDurationShouldBlock) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(4);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents(_))
      .Times(1);

  std::vector<UpdateSessionCallback> update_session_cb;
  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillRepeatedly(WithArg<0>([&](auto request) {
        update_session_cb.emplace_back(request->callback());
      }));
  SetSessionCaptionInitializer(/*success=*/true);
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                         base::DoNothing());
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          base::DoNothing());
  boca_app_handler()->ExtendSessionDuration(base::Minutes(2),
                                            base::DoNothing());
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                         base::DoNothing());

  ASSERT_THAT(update_session_cb, testing::SizeIs(1));
  std::move(update_session_cb[0]).Run(std::make_unique<::boca::Session>());
  ASSERT_THAT(update_session_cb, testing::SizeIs(2));
  std::move(update_session_cb[1]).Run(std::make_unique<::boca::Session>());
  ASSERT_THAT(update_session_cb, testing::SizeIs(3));
  std::move(update_session_cb[2]).Run(std::make_unique<::boca::Session>());
  ASSERT_THAT(update_session_cb, testing::SizeIs(4));
  std::move(update_session_cb[3]).Run(std::make_unique<::boca::Session>());

  // Called on destruction to disable session captions.
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents(_))
      .Times(1);
}

TEST_F(BocaAppPageHandlerProducerTest,
       ShouldNotUsePreviousSessionCaptionConfig) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(2);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents(_))
      .Times(1);
  std::unique_ptr<UpdateSessionRequest> first_request;
  std::unique_ptr<UpdateSessionRequest> second_request;

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(
          WithArg<0>([&](auto request) { first_request = std::move(request); }))
      .WillOnce(WithArg<0>(
          [&](auto request) { second_request = std::move(request); }));
  SetSessionCaptionInitializer(/*success=*/true);
  // Update caption config.
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          base::DoNothing());
  ::boca::Session response = GetCommonActiveSessionProto();
  *response.mutable_student_group_configs()
       ->at(kMainStudentGroupName)
       .mutable_captions_config() = *first_request->captions_config();
  first_request->callback().Run(
      std::make_unique<::boca::Session>(std::move(response)));
  // Close current session and start a new one.
  boca_app_handler()->OnSessionEnded(session.session_id());
  boca_app_handler()->OnSessionStarted("new-session-id",
                                       ::boca::UserIdentity());
  // Update ontask config.
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                         base::DoNothing());
  second_request->callback().Run(std::make_unique<::boca::Session>());
  ASSERT_THAT(second_request, testing::NotNull());
  EXPECT_NE(second_request->captions_config()->SerializeAsString(),
            first_request->captions_config()->SerializeAsString());
}

TEST_F(BocaAppPageHandlerProducerTest,
       ShouldNotUsePreviousSessionOnTaskConfig) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(2);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents(_))
      .Times(1);

  std::unique_ptr<UpdateSessionRequest> first_request;
  std::unique_ptr<UpdateSessionRequest> second_request;

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(
          WithArg<0>([&](auto request) { first_request = std::move(request); }))
      .WillOnce(WithArg<0>(
          [&](auto request) { second_request = std::move(request); }));
  // Update ontask config.
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                         base::DoNothing());
  ::boca::Session response = GetCommonActiveSessionProto();
  *response.mutable_student_group_configs()
       ->at(kMainStudentGroupName)
       .mutable_on_task_config() = *first_request->on_task_config();
  first_request->callback().Run(
      std::make_unique<::boca::Session>(std::move(response)));
  // Close current session and start a new one.
  boca_app_handler()->OnSessionEnded(session.session_id());
  boca_app_handler()->OnSessionStarted("new-session-id",
                                       ::boca::UserIdentity());
  // Update caption config.
  SetSessionCaptionInitializer(/*success=*/true);
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          base::DoNothing());

  second_request->callback().Run(std::make_unique<::boca::Session>());
  ASSERT_THAT(second_request, testing::NotNull());
  EXPECT_NE(second_request->on_task_config()->SerializeAsString(),
            first_request->on_task_config()->SerializeAsString());

  // Called on destruction to disable session captions.
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents(_))
      .Times(1);
}

TEST_F(BocaAppPageHandlerProducerTest,
       UpdateOnTaskConfigWithFailedCaptionConfigShouldUseSessionData) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  // Failed remote caption update should still dispatch local events.
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_2;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        // Use session on task config.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .on_task_config()
                      .SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        request->callback().Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
      }))
      .WillOnce(WithArg<0>([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonTestUnLockOnTaskConfigProto().SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        // Use session caption config.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .captions_config()
                      .SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        request->callback().Run(
            std::make_unique<::boca::Session>(GetCommonActiveSessionProto()));
      }));
  SetSessionCaptionInitializer(/*success=*/true);
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_1.GetCallback());
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                         future_2.GetCallback());

  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Wait());
  EXPECT_FALSE(future_2.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest,
       UpdateCaptionConfigWithFailedOnTaskConfigShouldUseSessionData) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  // Failed remote caption update should still dispatch local events.
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents(_))
      .Times(1);
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_2;

  UpdateSessionRequest request(nullptr, kTestUrlBase, session.teacher(),
                               session.session_id(), future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonTestUnLockOnTaskConfigProto().SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        // Use latest caption cofig value from session.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .captions_config()
                      .SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        request->callback().Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
      }))
      .WillOnce(WithArg<0>([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        // Use session on task config.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .on_task_config()
                      .SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        request->callback().Run(
            std::make_unique<::boca::Session>(GetCommonActiveSessionProto()));
      }));
  boca_app_handler()->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                         future_1.GetCallback());
  SetSessionCaptionInitializer(/*success=*/true);
  boca_app_handler()->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                          future_2.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Wait());
  EXPECT_FALSE(future_2.Get().has_value());

  // Called on destruction to disable session captions.
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents(_))
      .Times(1);
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateEmptyStudentActivitySucceed) {
  std::map<std::string, ::boca::StudentStatus> activities;
  base::test::TestFuture<std::vector<mojom::IdentifiedActivityPtr>> future;
  fake_page()->SetActivityInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnConsumerActivityUpdated(activities);
  auto result = future.Take();
  ASSERT_TRUE(result.empty());
}

TEST_F(BocaAppPageHandlerProducerTest, UpdateNonEmptyStudentActivitySucceed) {
  std::map<std::string, ::boca::StudentStatus> activities;
  ::boca::StudentStatus status_1;
  status_1.set_state(::boca::StudentStatus::ACTIVE);
  ::boca::StudentDevice device_1;
  auto* activity_1 = device_1.mutable_activity();
  activity_1->mutable_active_tab()->set_title("google");
  device_1.set_state(::boca::StudentDevice::INACTIVE);
  device_1.mutable_view_screen_config()
      ->mutable_connection_param()
      ->set_connection_code("abcd");
  (*status_1.mutable_devices())["device1"] = std::move(device_1);

  ::boca::StudentStatus status_2;
  status_2.set_state(::boca::StudentStatus::ADDED);
  ::boca::StudentDevice device_2;
  device_2.set_state(::boca::StudentDevice::ACTIVE);
  auto* activity_2 = device_2.mutable_activity();
  activity_2->mutable_active_tab()->set_title("youtube");
  (*status_2.mutable_devices())["device2"] = std::move(device_2);
  activities.emplace("1", std::move(status_1));
  activities.emplace("2", std::move(status_2));

  // EXPECT_CALL(mock_page(), OnStudentActivityUpdated(_)).Times(1);
  base::test::TestFuture<std::vector<mojom::IdentifiedActivityPtr>> future;
  fake_page()->SetActivityInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnConsumerActivityUpdated(activities);
  auto result = future.Take();
  EXPECT_EQ(2u, result.size());
  // Verify only first device added.
  EXPECT_EQ("1", result[0]->id);
  EXPECT_EQ(mojom::StudentStatusDetail::kActive,
            result[0]->activity->student_status_detail);
  EXPECT_FALSE(result[0]->activity->is_active);
  EXPECT_EQ("google", result[0]->activity->active_tab);
  // Connection code should be set
  EXPECT_EQ("abcd", result[0]->activity->view_screen_session_code);

  EXPECT_EQ("2", result[1]->id);
  EXPECT_EQ("youtube", result[1]->activity->active_tab);
  EXPECT_TRUE(result[1]->activity->is_active);
}

TEST_F(BocaAppPageHandlerProducerTest,
       UpdateStudentActivityWithEmptyDeviceStateSucceed) {
  std::map<std::string, ::boca::StudentStatus> activities;
  ::boca::StudentStatus status_1;
  status_1.set_state(::boca::StudentStatus::REMOVED_BY_OTHER_SESSION);
  activities.emplace("1", std::move(status_1));

  base::test::TestFuture<std::vector<mojom::IdentifiedActivityPtr>> future;
  fake_page()->SetActivityInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnConsumerActivityUpdated(activities);
  auto result = future.Take();
  EXPECT_EQ(1u, result.size());
  // Verify only first device added.
  EXPECT_EQ("1", result[0]->id);
  EXPECT_EQ(mojom::StudentStatusDetail::kRemovedByOtherSession,
            result[0]->activity->student_status_detail);
  EXPECT_FALSE(result[0]->activity->is_active);
  EXPECT_EQ("", result[0]->activity->active_tab);
  EXPECT_EQ("", result[0]->activity->view_screen_session_code);
}

TEST_F(BocaAppPageHandlerProducerTest,
       UpdateStudentActivityWithMultipleDevicesSignedInSucceed) {
  std::map<std::string, ::boca::StudentStatus> activities;
  ::boca::StudentStatus status_1;
  status_1.set_state(::boca::StudentStatus::ACTIVE);
  ::boca::StudentDevice device_1;
  device_1.set_state(::boca::StudentDevice::ACTIVE);
  (*status_1.mutable_devices())["device1"] = std::move(device_1);
  ::boca::StudentDevice device_2;
  device_2.set_state(::boca::StudentDevice::ACTIVE);
  (*status_1.mutable_devices())["device2"] = std::move(device_2);
  activities.emplace("1", std::move(status_1));

  base::test::TestFuture<std::vector<mojom::IdentifiedActivityPtr>> future;
  fake_page()->SetActivityInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnConsumerActivityUpdated(activities);
  auto result = future.Take();
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ("1", result[0]->id);
  EXPECT_EQ(mojom::StudentStatusDetail::kMultipleDeviceSignedIn,
            result[0]->activity->student_status_detail);
  EXPECT_FALSE(result[0]->activity->is_active);
  EXPECT_EQ("", result[0]->activity->active_tab);
  EXPECT_EQ("", result[0]->activity->view_screen_session_code);
}

TEST_F(BocaAppPageHandlerProducerTest,
       RemoveStudentSucceedAlsoRemoveFromLocalSession) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->set_session_state(::boca::Session::ACTIVE);
  auto* roster = session->mutable_roster();
  auto* student_groups = roster->mutable_student_groups()->Add();
  auto* student_1 = student_groups->mutable_students()->Add();
  student_1->set_gaia_id("2");

  auto* student_groups_2 = roster->mutable_student_groups()->Add();
  auto* student_3 = student_groups_2->mutable_students()->Add();
  student_3->set_gaia_id("4");
  auto* student_4 = student_groups_2->mutable_students()->Add();
  student_4->set_gaia_id("5");
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));

  // Page handler callback.
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::RemoveStudentError>> future_1;

  RemoveStudentRequest request(nullptr, kTestUrlBase, kGaiaId, session_id,
                               future.GetCallback());

  const char student_id[] = "4";
  EXPECT_CALL(*session_client_impl(), RemoveStudent(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId, request->gaia_id());
            ASSERT_EQ(1u, request->student_ids().size());
            ASSERT_EQ(student_id, request->student_ids()[0]);
            request->callback().Run(true);
          }));

  boca_app_handler()->RemoveStudent(student_id, future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
  EXPECT_EQ(2, session->roster().student_groups().size());
  EXPECT_EQ(1, session->roster().student_groups()[1].students().size());
  EXPECT_EQ("5", session->roster().student_groups()[1].students()[0].gaia_id());
}

TEST_F(BocaAppPageHandlerProducerTest, RemoveStudentWithHTTPFailure) {
  base::HistogramTester histogram_tester;
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));

  // Page handler callback.
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::RemoveStudentError>> future_1;

  RemoveStudentRequest request(nullptr, kTestUrlBase, kGaiaId, session_id,
                               future.GetCallback());

  const char student_id[] = "id";
  EXPECT_CALL(*session_client_impl(), RemoveStudent(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId, request->gaia_id());
            ASSERT_EQ(1u, request->student_ids().size());
            ASSERT_EQ(student_id, request->student_ids()[0]);
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          }));

  boca_app_handler()->RemoveStudent(student_id, future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  histogram_tester.ExpectTotalCount(kBocaRemoveStudentErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaRemoveStudentErrorCodeUmaPath,
                                     google_apis::ApiErrorCode::HTTP_FORBIDDEN,
                                     1);
}

TEST_F(BocaAppPageHandlerProducerTest, RemoveStudentWithEmptySession) {
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));

  // API callback.
  base::test::TestFuture<std::optional<mojom::RemoveStudentError>> future_1;

  boca_app_handler()->RemoveStudent("any", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::RemoveStudentError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerProducerTest, RemoveStudentWithNonActiveSession) {
  ::boca::Session session;
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));

  // API callback.
  base::test::TestFuture<std::optional<mojom::RemoveStudentError>> future_1;

  boca_app_handler()->RemoveStudent("any", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::RemoveStudentError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerProducerTest, RenotifyStudentSucceed) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));

  // Page handler callback.
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::RenotifyStudentError>> future_1;

  RenotifyStudentRequest request(nullptr, kTestUrlBase, kGaiaId, session_id,
                                 future.GetCallback());

  const char student_id[] = "4";
  EXPECT_CALL(*session_client_impl(), RenotifyStudent(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId, request->gaia_id());
            ASSERT_EQ(1u, request->student_ids().size());
            ASSERT_EQ(student_id, request->student_ids()[0]);
            request->callback().Run(true);
          }));

  boca_app_handler()->RenotifyStudent(student_id, future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, RenotifyStudentWithHTTPFailure) {
  base::HistogramTester histogram_tester;
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));

  // Page handler callback.
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::RenotifyStudentError>> future_1;

  RenotifyStudentRequest request(nullptr, kTestUrlBase, kGaiaId, session_id,
                                 future.GetCallback());

  const char student_id[] = "id";
  EXPECT_CALL(*session_client_impl(), RenotifyStudent(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId, request->gaia_id());
            ASSERT_EQ(1u, request->student_ids().size());
            ASSERT_EQ(student_id, request->student_ids()[0]);
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          }));

  boca_app_handler()->RenotifyStudent(student_id, future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, RenotifyStudentWithEmptySession) {
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));

  // API callback.
  base::test::TestFuture<std::optional<mojom::RenotifyStudentError>> future_1;

  boca_app_handler()->RenotifyStudent("any", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::RenotifyStudentError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerProducerTest, RenotifyStudentWithNonActiveSession) {
  ::boca::Session session;
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));

  // API callback.
  base::test::TestFuture<std::optional<mojom::RenotifyStudentError>> future_1;

  boca_app_handler()->RenotifyStudent("any", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::RenotifyStudentError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerProducerTest,
       AddStudentsSucceedAlsoTriggerSessionReload) {
  const auto* session_id = "123";
  const auto* group_id = "groupId";
  auto session = std::make_unique<::boca::Session>();
  session->set_session_state(::boca::Session::ACTIVE);
  auto* roster = session->mutable_roster();
  auto* student_groups = roster->mutable_student_groups()->Add();
  student_groups->set_student_group_id(group_id);
  auto* student_1 = student_groups->mutable_students()->Add();
  student_1->set_gaia_id("2");

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));

  // Page handler callback.
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::AddStudentsError>> future_1;

  AddStudentsRequest request(nullptr, kTestUrlBase, kGaiaId, session_id,
                             future.GetCallback());

  EXPECT_CALL(*session_client_impl(), AddStudents(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId, request->gaia_id());
            ASSERT_EQ(2u, request->students().size());
            ASSERT_EQ(group_id, request->student_group_id());
            EXPECT_EQ("1", request->students()[0].gaia_id());
            EXPECT_EQ("a", request->students()[0].full_name());
            EXPECT_EQ("a@gmail.com", request->students()[0].email());
            EXPECT_EQ("cdn://s1", request->students()[0].photo_url());

            EXPECT_EQ("2", request->students()[1].gaia_id());
            EXPECT_EQ("b", request->students()[1].full_name());
            EXPECT_EQ("b@gmail.com", request->students()[1].email());
            EXPECT_EQ("cdn://s2", request->students()[1].photo_url());
            request->callback().Run(true);
          }));
  EXPECT_CALL(*session_manager(), LoadCurrentSession(/*from_polling=*/false))
      .Times(1);
  std::vector<mojom::IdentityPtr> students;
  students.push_back(
      mojom::Identity::New("1", "a", "a@gmail.com", GURL("cdn://s1")));
  students.push_back(
      mojom::Identity::New("2", "b", "b@gmail.com", GURL("cdn://s2")));
  boca_app_handler()->AddStudents(std::move(students), future_1.GetCallback());
  ::testing::Mock::VerifyAndClearExpectations(session_client_impl());
  ::testing::Mock::VerifyAndClearExpectations(session_manager());

  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, AddStudentsWithHTTPFailure) {
  base::HistogramTester histogram_tester;
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));

  // Page handler callback.
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::AddStudentsError>> future_1;

  AddStudentsRequest request(nullptr, kTestUrlBase, kGaiaId, session_id,
                             future.GetCallback());

  EXPECT_CALL(*session_client_impl(), AddStudents(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            ASSERT_EQ(kGaiaId, request->gaia_id());
            ASSERT_EQ(0u, request->students().size());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          }));
  EXPECT_CALL(*session_manager(), LoadCurrentSession(/*from_polling=*/false))
      .Times(0);
  boca_app_handler()->AddStudents({}, future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  histogram_tester.ExpectTotalCount(kBocaAddStudentsErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaAddStudentsErrorCodeUmaPath,
                                     google_apis::ApiErrorCode::HTTP_FORBIDDEN,
                                     1);
}

TEST_F(BocaAppPageHandlerProducerTest, OnSessionSessionStartedSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  fake_page()->SetSessionConfigInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnSessionStarted(std::string(), ::boca::UserIdentity());
  auto result = future.Take();
  ASSERT_TRUE(result->is_config());
}

TEST_F(BocaAppPageHandlerProducerTest, OnSessionSessionMetadataUpdatedSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  fake_page()->SetSessionConfigInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnSessionMetadataUpdated(std::string());
  auto result = future.Take();
  ASSERT_TRUE(result->is_config());
}

TEST_F(BocaAppPageHandlerProducerTest, OnSessionEndedSucceed) {
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  fake_page()->SetSessionConfigInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnSessionEnded("any");
  auto result = future.Take();
  ASSERT_TRUE(result->is_error());
}

TEST_F(BocaAppPageHandlerProducerTest, OnSessionCaptionUpdatedSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  fake_page()->SetSessionConfigInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnSessionCaptionConfigUpdated(
      "any", ::boca::CaptionsConfig(), std::string());
  auto result = future.Take();
  ASSERT_TRUE(result->is_config());
}

TEST_F(BocaAppPageHandlerProducerTest, OnSessionBundleUpdatedSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  fake_page()->SetSessionConfigInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnBundleUpdated(::boca::Bundle());
  auto result = future.Take();
  ASSERT_TRUE(result->is_config());
}

TEST_F(BocaAppPageHandlerProducerTest, OnSessionRosterUpdatedSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<mojom::ConfigResultPtr> future;
  fake_page()->SetSessionConfigInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnSessionRosterUpdated({});
  auto result = future.Take();
  ASSERT_TRUE(result->is_config());
}

TEST_F(BocaAppPageHandlerProducerTest, JoinSessionSucceeded) {
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::SubmitAccessCodeError>> future_1;

  JoinSessionRequest request(nullptr, kTestUrlBase, ::boca::UserIdentity(),
                             "device", "code", future.GetCallback());

  EXPECT_CALL(*session_client_impl(), JoinSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            request->callback().Run(std::make_unique<::boca::Session>());
          }));
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));

  boca_app_handler()->SubmitAccessCode("code", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, JoinSessionFailed) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(0);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::SubmitAccessCodeError>> future_1;

  JoinSessionRequest request(nullptr, kTestUrlBase, ::boca::UserIdentity(),
                             "device", "code", future.GetCallback());

  EXPECT_CALL(*session_client_impl(), JoinSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          [&](auto request) {
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          }));
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(false));

  boca_app_handler()->SubmitAccessCode("code", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::SubmitAccessCodeError::kInvalid, future_1.Get().value());
  histogram_tester.ExpectTotalCount(
      kBocaJoinSessionViaAccessCodeErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaJoinSessionViaAccessCodeErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_FORBIDDEN, 1);
}

TEST_F(BocaAppPageHandlerProducerTest,
       JoinSessionFailedDueToNonManagedNetwork) {
  EXPECT_CALL(*session_manager(),
              UpdateCurrentSession(_, /*dispatch_event=*/true))
      .Times(0);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::SubmitAccessCodeError>> future_1;

  JoinSessionRequest request(nullptr, kTestUrlBase, ::boca::UserIdentity(),
                             "device", "code", future.GetCallback());

  EXPECT_CALL(*session_client_impl(), JoinSession(_)).Times(0);
  EXPECT_CALL(*session_manager(), disabled_on_non_managed_network())
      .WillOnce(Return(true));
  boca_app_handler()->SubmitAccessCode("code", future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::SubmitAccessCodeError::kNetworkRestriction,
            future_1.Get().value());
}

TEST_F(BocaAppPageHandlerProducerTest, ViewScreenSucceeded) {
  const std::string student_id = "123";
  EXPECT_CALL(*spotlight_service(), ViewScreen(student_id, kTestUrlBase, _))
      .WillOnce(WithArg<2>(
          [&](auto request) { std::move(request).Run(base::ok(true)); }));

  base::test::TestFuture<std::optional<mojom::ViewStudentScreenError>> future;

  boca_app_handler()->ViewStudentScreen(student_id, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, ViewScreenFailed) {
  base::HistogramTester histogram_tester;
  const std::string student_id = "123";

  EXPECT_CALL(*spotlight_service(), ViewScreen(student_id, kTestUrlBase, _))
      .WillOnce(WithArg<2>([&](auto request) {
        std::move(request).Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
      }));

  base::test::TestFuture<std::optional<mojom::ViewStudentScreenError>> future;

  boca_app_handler()->ViewStudentScreen(student_id, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(mojom::ViewStudentScreenError::kHTTPError, future.Get().value());

  histogram_tester.ExpectTotalCount(
      kBocaSpotlightViewStudentScreenErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaSpotlightViewStudentScreenErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_FORBIDDEN, 1);
}

TEST_F(BocaAppPageHandlerProducerTest, AuthenticateWebviewSuccess) {
  EXPECT_CALL(*webview_auth_handler(), AuthenticateWebview(testing::_))
      .WillOnce(base::test::RunOnceCallback<0>(/*is_success=*/true));
  base::RunLoop run_loop;
  boca_app_handler()->AuthenticateWebview(
      base::BindLambdaForTesting([&](bool success) -> void {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BocaAppPageHandlerProducerTest, AuthenticateWebviewFailure) {
  EXPECT_CALL(*webview_auth_handler(), AuthenticateWebview(testing::_))
      .WillOnce(base::test::RunOnceCallback<0>(/*is_success=*/false));
  base::RunLoop run_loop;
  boca_app_handler()->AuthenticateWebview(
      base::BindLambdaForTesting([&](bool success) -> void {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BocaAppPageHandlerProducerTest, TestPrefGetter) {
  boca_app_handler()->SetPrefForTesting(pref_service());
  base::test::TestFuture<base::Value> future;
  boca_app_handler()->GetUserPref(
      mojom::BocaValidPref::kDefaultMediaStreamSetting, future.GetCallback());
  EXPECT_TRUE(future.Get().is_int());
}

TEST_F(BocaAppPageHandlerProducerTest, TestPrefGetterAndSetter) {
  base::Value::Dict nav_map;
  base::Value::Dict nav_occurrence;
  nav_occurrence.Set("navRule", 0);
  nav_occurrence.Set("occurence", 1);
  nav_map.Set("google.com", std::move(nav_occurrence));
  TestUserPref(mojom::BocaValidPref::kNavigationSetting,
               /*value=*/base::Value(nav_map.Clone()));
}

TEST_F(BocaAppPageHandlerProducerTest, EndViewScreenSessionSucceeded) {
  const std::string student_id = "123";
  EXPECT_CALL(*session_manager(), EndSpotlightSession).Times(1);
  EXPECT_CALL(
      *spotlight_service(),
      UpdateViewScreenState(student_id, ::boca::ViewScreenConfig::INACTIVE,
                            kTestUrlBase, _))
      .WillOnce(WithArg<3>(
          [&](auto request) { std::move(request).Run(base::ok(true)); }));

  base::test::TestFuture<std::optional<mojom::EndViewScreenSessionError>>
      future;

  boca_app_handler()->EndViewScreenSession(student_id, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest,
       EndViewScreenSessionWhilePresentingStudentScreen) {
  const std::string_view kEndViewScreenStudentId = "student-id";
  base::test::TestFuture<std::optional<mojom::EndViewScreenSessionError>>
      future;
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  EXPECT_CALL(
      *student_screen_presenter,
      IsPresenting(std::optional<std::string_view>(kEndViewScreenStudentId)))
      .WillOnce(Return(true));
  EXPECT_CALL(*session_manager(), EndSpotlightSession).Times(0);
  EXPECT_CALL(*spotlight_service(), UpdateViewScreenState).Times(0);

  boca_app_handler()->EndViewScreenSession(std::string(kEndViewScreenStudentId),
                                           future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, EndViewScreenSessionFailed) {
  base::HistogramTester histogram_tester;
  const std::string student_id = "123";

  EXPECT_CALL(*session_manager(), EndSpotlightSession).Times(1);
  EXPECT_CALL(
      *spotlight_service(),
      UpdateViewScreenState(student_id, ::boca::ViewScreenConfig::INACTIVE,
                            kTestUrlBase, _))
      .WillOnce(WithArg<3>([&](auto request) {
        std::move(request).Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
      }));

  base::test::TestFuture<std::optional<mojom::EndViewScreenSessionError>>
      future;

  boca_app_handler()->EndViewScreenSession(student_id, future.GetCallback());
  EXPECT_EQ(mojom::EndViewScreenSessionError::kHTTPError, future.Get().value());

  histogram_tester.ExpectTotalCount(
      kBocaSpotlightEndViewStudentScreenErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaSpotlightEndViewStudentScreenErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_FORBIDDEN, 1);
}

TEST_F(BocaAppPageHandlerProducerTest, OpenFeedbackDialog) {
  EXPECT_CALL(*boca_app_client(), OpenFeedbackDialog()).Times(1);
  base::test::TestFuture<void> open_feedback_future;
  boca_app_handler()->OpenFeedbackDialog(open_feedback_future.GetCallback());
  EXPECT_TRUE(open_feedback_future.Wait());
}

TEST_F(BocaAppPageHandlerProducerTest, RefreshWorkbook) {
  EXPECT_CALL(*session_manager(), NotifyAppReload()).Times(1);
  base::test::TestFuture<void> refresh_workbook_future;
  boca_app_handler()->RefreshWorkbook(refresh_workbook_future.GetCallback());
  EXPECT_TRUE(refresh_workbook_future.Wait());
}

TEST_F(BocaAppPageHandlerProducerTest, SetViewScreenSessionActiveSucceeded) {
  const std::string student_id = "123";
  EXPECT_CALL(
      *spotlight_service(),
      UpdateViewScreenState(student_id, ::boca::ViewScreenConfig::ACTIVE,
                            kTestUrlBase, _))
      .WillOnce(WithArg<3>(
          [&](auto request) { std::move(request).Run(base::ok(true)); }));

  base::test::TestFuture<std::optional<mojom::SetViewScreenSessionActiveError>>
      future;

  boca_app_handler()->SetViewScreenSessionActive(student_id,
                                                 future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(BocaAppPageHandlerProducerTest, SetViewScreenSessionActiveFailed) {
  base::HistogramTester histogram_tester;
  const std::string student_id = "123";

  EXPECT_CALL(
      *spotlight_service(),
      UpdateViewScreenState(student_id, ::boca::ViewScreenConfig::ACTIVE,
                            kTestUrlBase, _))
      .WillOnce(WithArg<3>([&](auto request) {
        std::move(request).Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
      }));

  base::test::TestFuture<std::optional<mojom::SetViewScreenSessionActiveError>>
      future;

  boca_app_handler()->SetViewScreenSessionActive(student_id,
                                                 future.GetCallback());
  EXPECT_EQ(mojom::SetViewScreenSessionActiveError::kHTTPError,
            future.Get().value());
  histogram_tester.ExpectTotalCount(
      kBocaSpotlightSetViewScreenSessionActiveErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaSpotlightSetViewScreenSessionActiveErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_FORBIDDEN, 1);
}

class BocaAppPageHandlerFloatModeTest : public AshTestBase {
 public:
  BocaAppPageHandlerFloatModeTest() = default;
  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
  }
};

TEST_F(BocaAppPageHandlerFloatModeTest, SetFloatModeTest) {
  UpdateDisplay("1366x768");
  std::unique_ptr<aura::Window> window = CreateToplevelTestWindow(
      gfx::Rect(800, 200, 500, 150), desks_util::GetActiveDeskContainerId());

  base::test::TestFuture<bool> future;
  BocaAppHandler::SetFloatModeAndBoundsForWindow(true, window.get(),
                                                 future.GetCallback());

  // TODO(crbug.com/374881187): We don't have a way to verify float state in
  // unit test, verify bounds for now. Move to browser test in the future.
  // WindowState* window_state = WindowState::Get(window.get());
  // EXPECT_TRUE(window_state->IsFloated());
  EXPECT_EQ(400, window->bounds().width());
  EXPECT_EQ(600, window->bounds().height());
  EXPECT_EQ(958, window->bounds().x());
  EXPECT_EQ(8, window->bounds().y());
  EXPECT_TRUE(future.Get());
}

TEST_F(BocaAppPageHandlerFloatModeTest, SetFloatModeTestWithFalse) {
  base::test::TestFuture<bool> future;
  BocaAppHandler::SetFloatModeAndBoundsForWindow(false, nullptr,
                                                 future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest, NotifyWhenLocalCaptionClosed) {
  base::test::TestFuture<void> future;
  fake_page()->SetLocalCaptionDisabledInterceptorCallback(future.GetCallback());
  boca_app_handler()->OnLocalCaptionClosed();
  EXPECT_TRUE(future.Wait());
}

TEST_F(BocaAppPageHandlerProducerTest,
       NotifyWhenSessionCaptionClosedRequestSucceed) {
  ::boca::CaptionsConfig request_captions_config;
  ::boca::CaptionsConfig notify_captions_config;
  ::boca::Session session =
      GetCommonActiveSessionProto({.captions_enabled = true});
  EXPECT_CALL(*session_manager(), GetCurrentSession)
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession).Times(1);
  EXPECT_CALL(*session_client_impl(), UpdateSession)
      .WillOnce([&request_captions_config](
                    std::unique_ptr<UpdateSessionRequest> request) {
        std::unique_ptr<::boca::Session> result =
            std::make_unique<::boca::Session>(
                GetCommonActiveSessionProto({.captions_enabled = false}));
        request_captions_config = *request->captions_config();
        request->callback().Run(std::move(result));
      });
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents)
      .WillOnce([&notify_captions_config](
                    const ::boca::CaptionsConfig& captions_config) {
        notify_captions_config = captions_config;
      });
  base::test::TestFuture<bool> future;
  fake_page()->SetSessionCaptionDisabledInterceptorCallback(
      future.GetCallback());
  boca_app_handler()->OnSessionCaptionClosed(/*is_error=*/false);

  EXPECT_FALSE(request_captions_config.captions_enabled());
  EXPECT_FALSE(notify_captions_config.captions_enabled());
  EXPECT_FALSE(future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest,
       NotifyWhenSessionCaptionClosedRequestFailed) {
  ::boca::CaptionsConfig request_captions_config;
  ::boca::CaptionsConfig notify_captions_config;
  ::boca::Session session =
      GetCommonActiveSessionProto({.captions_enabled = true});
  EXPECT_CALL(*session_manager(), GetCurrentSession)
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession).Times(0);
  EXPECT_CALL(*session_client_impl(), UpdateSession)
      .WillOnce([&request_captions_config](
                    std::unique_ptr<UpdateSessionRequest> request) {
        std::unique_ptr<::boca::Session> result =
            std::make_unique<::boca::Session>(
                GetCommonActiveSessionProto({.captions_enabled = false}));
        request_captions_config = *request->captions_config();
        request->callback().Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_BAD_REQUEST));
      });
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents)
      .WillOnce([&notify_captions_config](
                    const ::boca::CaptionsConfig& captions_config) {
        notify_captions_config = captions_config;
      });
  base::test::TestFuture<bool> future;
  fake_page()->SetSessionCaptionDisabledInterceptorCallback(
      future.GetCallback());
  boca_app_handler()->OnSessionCaptionClosed(/*is_error=*/true);

  EXPECT_FALSE(request_captions_config.captions_enabled());
  EXPECT_FALSE(notify_captions_config.captions_enabled());
  EXPECT_TRUE(future.Get());
}

TEST_F(BocaAppPageHandlerConsumerTest,
       DoesNotNotifyWhenSessionCaptionClosedForConsumer) {
  ::boca::Session session =
      GetCommonActiveSessionProto({.captions_enabled = true});
  EXPECT_CALL(*session_manager(), GetCurrentSession)
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession).Times(0);
  EXPECT_CALL(*session_client_impl(), UpdateSession).Times(0);
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents).Times(0);
  base::test::TestFuture<bool> future;
  fake_page()->SetSessionCaptionDisabledInterceptorCallback(
      future.GetCallback());
  boca_app_handler()->OnSessionCaptionClosed(/*is_error=*/true);

  EXPECT_FALSE(future.IsReady());
}

TEST_F(BocaAppPageHandlerProducerTest,
       DoesNotNotifyWhenSessionCaptionClosedIfNullSession) {
  EXPECT_CALL(*session_manager(), GetCurrentSession)
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession).Times(0);
  EXPECT_CALL(*session_client_impl(), UpdateSession).Times(0);
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents).Times(0);
  base::test::TestFuture<bool> future;
  fake_page()->SetSessionCaptionDisabledInterceptorCallback(
      future.GetCallback());
  boca_app_handler()->OnSessionCaptionClosed(/*is_error=*/true);

  EXPECT_FALSE(future.IsReady());
}

TEST_F(BocaAppPageHandlerProducerTest,
       DoesNotNotifyWhenSessionCaptionClosedIfSessionInactive) {
  ::boca::Session session =
      GetCommonActiveSessionProto({.captions_enabled = true});
  session.set_session_state(::boca::Session::PAST);
  EXPECT_CALL(*session_manager(), GetCurrentSession)
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession).Times(0);
  EXPECT_CALL(*session_client_impl(), UpdateSession).Times(0);
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents).Times(0);
  base::test::TestFuture<bool> future;
  fake_page()->SetSessionCaptionDisabledInterceptorCallback(
      future.GetCallback());
  boca_app_handler()->OnSessionCaptionClosed(/*is_error=*/true);

  EXPECT_FALSE(future.IsReady());
}

TEST_F(BocaAppPageHandlerProducerTest,
       ProducerCaptionsOverrideGetSessionCaptions) {
  ::boca::Session response_session = GetCommonActiveSessionProto(
      {.captions_enabled = true, .translations_enabled = true});
  auto current_session = std::make_unique<::boca::Session>();
  PrepareGetSession(current_session.get(), response_session);

  base::test::TestFuture<mojom::SessionResultPtr> future;
  CreateBocaAppHandler(/*is_producer=*/true);
  boca_app_handler()->GetSession(future.GetCallback());
  mojom::SessionResultPtr get_session_result = future.Take();

  ASSERT_TRUE(get_session_result->is_session());
  EXPECT_FALSE(get_session_result->get_session()
                   ->config->caption_config->session_caption_enabled);
  EXPECT_FALSE(get_session_result->get_session()
                   ->config->caption_config->session_translation_enabled);
}

TEST_F(BocaAppPageHandlerProducerTest,
       ProducerCaptionsOverrideGetSessionCaptionsAfterUpdate) {
  ::boca::Session response_session = GetCommonActiveSessionProto();
  auto current_session =
      std::make_unique<::boca::Session>(GetCommonActiveSessionProto());
  ::boca::CaptionsConfig captions_notified;
  PrepareGetSession(current_session.get(), response_session);
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents).Times(1);
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents)
      .WillOnce(
          [&captions_notified](const ::boca::CaptionsConfig& captions_config) {
            captions_notified = captions_config;
          });
  EXPECT_CALL(*session_client_impl(), UpdateSession)
      .WillOnce([](std::unique_ptr<UpdateSessionRequest> request) {
        std::unique_ptr<::boca::Session> result =
            std::make_unique<::boca::Session>(GetCommonActiveSessionProto(
                {.captions_enabled = true, .translations_enabled = true}));
        request->callback().Run(std::move(result));
      });

  base::test::TestFuture<mojom::SessionResultPtr> future;
  SetSessionCaptionInitializer(/*success=*/true);
  boca_app_handler()->UpdateCaptionConfig(
      mojom::CaptionConfig::New(/*session_caption_enabled=*/true,
                                /*local_caption_enabled=*/true,
                                /*=session_translation_enabled=*/true),
      base::DoNothing());
  boca_app_handler()->GetSession(future.GetCallback());
  mojom::SessionResultPtr get_session_result = future.Take();

  EXPECT_TRUE(captions_notified.captions_enabled());
  EXPECT_TRUE(captions_notified.translations_enabled());
  ASSERT_TRUE(get_session_result->is_session());
  EXPECT_TRUE(get_session_result->get_session()
                  ->config->caption_config->session_caption_enabled);
  EXPECT_TRUE(get_session_result->get_session()
                  ->config->caption_config->session_translation_enabled);

  // Called on destruction to disable session captions.
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents(_))
      .Times(1);
}

TEST_F(BocaAppPageHandlerConsumerTest,
       ConsumerCaptionsDoesNotOverrideGetSessionCaptions) {
  ::boca::Session response_session = GetCommonActiveSessionProto(
      {.captions_enabled = true, .translations_enabled = true});
  auto current_session = std::make_unique<::boca::Session>();
  PrepareGetSession(current_session.get(), response_session);

  base::test::TestFuture<mojom::SessionResultPtr> future;
  boca_app_handler()->GetSession(future.GetCallback());
  mojom::SessionResultPtr get_session_result = future.Take();

  ASSERT_TRUE(get_session_result->is_session());
  EXPECT_TRUE(get_session_result->get_session()
                  ->config->caption_config->session_caption_enabled);
  EXPECT_TRUE(get_session_result->get_session()
                  ->config->caption_config->session_translation_enabled);
}

TEST_F(BocaAppPageHandlerProducerTest,
       EnableSessionCaptionRequestErrorWillFail) {
  base::HistogramTester histogram_tester;
  ::boca::Session response_session = GetCommonActiveSessionProto();
  auto current_session =
      std::make_unique<::boca::Session>(GetCommonActiveSessionProto());
  PrepareGetSession(current_session.get(), response_session);
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents).Times(1);
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents).Times(0);
  EXPECT_CALL(*session_client_impl(), UpdateSession)
      .WillOnce([](std::unique_ptr<UpdateSessionRequest> request) {
        request->callback().Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_BAD_REQUEST));
      });

  base::test::TestFuture<mojom::SessionResultPtr> get_future;
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>>
      update_future;
  SetSessionCaptionInitializer(/*success=*/true);
  boca_app_handler()->UpdateCaptionConfig(
      mojom::CaptionConfig::New(/*session_caption_enabled=*/true,
                                /*local_caption_enabled=*/true,
                                /*=session_translation_enabled=*/true),
      update_future.GetCallback());
  std::optional<mojom::UpdateSessionError> update_error = update_future.Take();
  boca_app_handler()->GetSession(get_future.GetCallback());
  mojom::SessionResultPtr get_session_result = get_future.Take();

  EXPECT_TRUE(update_error.has_value());
  ASSERT_TRUE(get_session_result->is_session());
  EXPECT_FALSE(get_session_result->get_session()
                   ->config->caption_config->session_caption_enabled);
  EXPECT_FALSE(get_session_result->get_session()
                   ->config->caption_config->session_translation_enabled);
  histogram_tester.ExpectTotalCount(UpdateCaptionErrorCodeUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      UpdateCaptionErrorCodeUmaPath,
      google_apis::ApiErrorCode::HTTP_BAD_REQUEST, 1);
}

TEST_F(BocaAppPageHandlerProducerTest,
       IgnoreDisableSessionCaptionRequestError) {
  ::boca::CaptionsConfig enable_captions_notified;
  ::boca::CaptionsConfig disable_captions_notified;
  ::boca::Session response_session = GetCommonActiveSessionProto(
      {.captions_enabled = true, .translations_enabled = true});
  auto current_session =
      std::make_unique<::boca::Session>(GetCommonActiveSessionProto());
  PrepareGetSession(current_session.get(), response_session);
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents).Times(2);

  // Simulate enable session captions success.
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents)
      .WillOnce([&enable_captions_notified](
                    const ::boca::CaptionsConfig& captions_config) {
        enable_captions_notified = captions_config;
      });
  EXPECT_CALL(*session_client_impl(), UpdateSession)
      .WillOnce([](std::unique_ptr<UpdateSessionRequest> request) {
        request->callback().Run(
            std::make_unique<::boca::Session>(GetCommonActiveSessionProto(
                {.captions_enabled = true, .translations_enabled = true})));
      });
  SetSessionCaptionInitializer(/*success=*/true);
  boca_app_handler()->UpdateCaptionConfig(
      mojom::CaptionConfig::New(/*session_caption_enabled=*/true,
                                /*local_caption_enabled=*/false,
                                /*=session_translation_enabled=*/true),
      base::DoNothing());

  // Simulate disable session captions error.
  EXPECT_CALL(*session_manager(), NotifySessionCaptionProducerEvents)
      .WillOnce([&disable_captions_notified](
                    const ::boca::CaptionsConfig& captions_config) {
        disable_captions_notified = captions_config;
      });
  EXPECT_CALL(*session_client_impl(), UpdateSession)
      .WillOnce([](std::unique_ptr<UpdateSessionRequest> request) {
        request->callback().Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_BAD_REQUEST));
      });
  base::test::TestFuture<mojom::SessionResultPtr> get_future;
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>>
      update_future;
  boca_app_handler()->UpdateCaptionConfig(
      mojom::CaptionConfig::New(/*session_caption_enabled=*/false,
                                /*local_caption_enabled=*/false,
                                /*=session_translation_enabled=*/true),
      update_future.GetCallback());
  std::optional<mojom::UpdateSessionError> update_error = update_future.Take();
  boca_app_handler()->GetSession(get_future.GetCallback());
  mojom::SessionResultPtr get_session_result = get_future.Take();

  EXPECT_FALSE(update_error.has_value());
  ASSERT_TRUE(get_session_result->is_session());
  EXPECT_FALSE(get_session_result->get_session()
                   ->config->caption_config->session_caption_enabled);
  EXPECT_TRUE(get_session_result->get_session()
                  ->config->caption_config->session_translation_enabled);
  EXPECT_TRUE(enable_captions_notified.captions_enabled());
  EXPECT_TRUE(enable_captions_notified.translations_enabled());
  EXPECT_FALSE(disable_captions_notified.captions_enabled());
  EXPECT_TRUE(disable_captions_notified.translations_enabled());
}

TEST_F(BocaAppPageHandlerProducerTest, PresentStudentScreenSuccess) {
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  base::OnceClosure disconnected_callback;
  base::test::TestFuture<bool> success_future;
  base::test::TestFuture<void> disconnected_future;
  mojom::IdentityPtr student_identity_mojom = mojom::Identity::New(
      kActiveStudentId, "student name", "student@email.com", std::nullopt);
  ::boca::UserIdentity student_identity;

  // Simulate existence of another `BocaAppHandler` instance to verify that it
  // will receive the disconnected event.
  EXPECT_CALL(*boca_app_client(), GetAppInstanceCount).WillOnce(Return(2));
  base::test::TestFuture<void> second_disconnected_future;
  mojo::Remote<mojom::PageHandler> second_remote;
  std::unique_ptr<FakePage> second_fake_page;
  std::unique_ptr<BocaAppHandler> second_boca_app_handler =
      CreateNewBocaAppHandler(/*is_producer=*/true, &second_remote,
                              &second_fake_page);

  ::boca::Session session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*session_manager(), EndSpotlightSession)
      .WillOnce([](base::OnceClosure callback) { std::move(callback).Run(); });
  EXPECT_CALL(*spotlight_service(), UpdateViewScreenState)
      .WillOnce([](std::string, ::boca::ViewScreenConfig::ViewScreenState,
                   std::string, ViewScreenRequestCallback callback) {
        std::move(callback).Run(true);
      });
  EXPECT_CALL(*session_manager(), GetStudentActiveDeviceId)
      .WillOnce(Return(kStudentDeviceId));
  EXPECT_CALL(*student_screen_presenter,
              Start(kReceiverId, _, kStudentDeviceId, _, _))
      .WillOnce([&disconnected_callback, &student_identity](
                    std::string_view, const ::boca::UserIdentity& student,
                    std::string_view, base::OnceCallback<void(bool)> success_cb,
                    base::OnceClosure disconnected_cb) {
        student_identity = std::move(student);
        disconnected_callback = std::move(disconnected_cb);
        std::move(success_cb).Run(true);
      });
  boca_app_handler()->PresentStudentScreen(student_identity_mojom->Clone(),
                                           kReceiverId,
                                           success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());
  EXPECT_EQ(student_identity.gaia_id(), student_identity_mojom->id);
  EXPECT_EQ(student_identity.full_name(), student_identity_mojom->name);
  EXPECT_EQ(student_identity.email(), student_identity_mojom->email);

  fake_page()->SetPresentStudentScreenEndedInterceptorCallback(
      disconnected_future.GetCallback());
  second_fake_page->SetPresentStudentScreenEndedInterceptorCallback(
      second_disconnected_future.GetCallback());
  std::move(disconnected_callback).Run();
  EXPECT_TRUE(disconnected_future.Wait());
  EXPECT_TRUE(second_disconnected_future.Wait());
}

TEST_F(BocaAppPageHandlerProducerTest, PresentStudentScreenFailure) {
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  base::test::TestFuture<bool> success_future;
  ::boca::Session session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*session_manager(), EndSpotlightSession)
      .WillOnce([](base::OnceClosure callback) { std::move(callback).Run(); });
  EXPECT_CALL(*spotlight_service(), UpdateViewScreenState)
      .WillOnce([](std::string, ::boca::ViewScreenConfig::ViewScreenState,
                   std::string, ViewScreenRequestCallback callback) {
        std::move(callback).Run(true);
      });
  EXPECT_CALL(*session_manager(), GetStudentActiveDeviceId)
      .WillOnce(Return(kStudentDeviceId));
  EXPECT_CALL(*student_screen_presenter,
              Start(kReceiverId, _, kStudentDeviceId, _, _))
      .WillOnce([](std::string_view, const ::boca::UserIdentity&,
                   std::string_view, base::OnceCallback<void(bool)> success_cb,
                   base::OnceClosure disconnected_cb) {
        std::move(success_cb).Run(false);
      });
  boca_app_handler()->PresentStudentScreen(
      mojom::Identity::New(kActiveStudentId, "student name",
                           "student@email.com", std::nullopt),
      kReceiverId, success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest,
       PresentStudentScreenWhilePresentingTeacherScreen) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> success_future;
  ::boca::Session session = GetCommonActiveSessionProto();
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  auto teacher_screen_presenter =
      std::make_unique<MockTeacherScreenPresenter>();
  ON_CALL(*session_manager(), GetTeacherScreenPresenter)
      .WillByDefault(Return(teacher_screen_presenter.get()));
  EXPECT_CALL(*teacher_screen_presenter, IsPresenting).WillOnce(Return(true));
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  boca_app_handler()->PresentStudentScreen(
      mojom::Identity::New(kActiveStudentId, "student name",
                           "student@email.com", std::nullopt),
      kReceiverId, success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
  histogram_tester.ExpectTotalCount(
      kBocaPresentStudentScreenFailureReasonUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentStudentScreenFailureReasonUmaPath,
      /* kTeacherScreenShareActive */ 2, 1);
  histogram_tester.ExpectTotalCount(kBocaPresentStudentScreenResultUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaPresentStudentScreenResultUmaPath,
                                     /* failure*/ 0, 1);
}

TEST_F(BocaAppPageHandlerProducerTest,
       PresentTeacherScreenBeforeCompletingPresentStudentScreen) {
  base::test::TestFuture<bool> success_future;
  base::test::TestFuture<ViewScreenRequestCallback> update_view_screen_future;
  ::boca::Session session = GetCommonActiveSessionProto();
  auto teacher_screen_presenter =
      std::make_unique<MockTeacherScreenPresenter>();
  ON_CALL(*session_manager(), GetTeacherScreenPresenter)
      .WillByDefault(Return(teacher_screen_presenter.get()));
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  ON_CALL(*session_manager(), GetStudentActiveDeviceId)
      .WillByDefault(Return(kStudentDeviceId));

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*session_manager(), EndSpotlightSession)
      .WillOnce([](base::OnceClosure callback) { std::move(callback).Run(); });
  EXPECT_CALL(*spotlight_service(), UpdateViewScreenState)
      .WillOnce([&update_view_screen_future](
                    std::string, ::boca::ViewScreenConfig::ViewScreenState,
                    std::string, ViewScreenRequestCallback callback) {
        update_view_screen_future.GetCallback().Run(std::move(callback));
      });
  EXPECT_CALL(*student_screen_presenter, Start).Times(0);
  // Initially simulate that there is no teacher screen presentation in
  // progress.
  EXPECT_CALL(*teacher_screen_presenter, IsPresenting).WillOnce(Return(false));
  boca_app_handler()->PresentStudentScreen(
      mojom::Identity::New(kActiveStudentId, "student name",
                           "student@email.com", std::nullopt),
      kReceiverId, success_future.GetCallback());
  // Simulate teacher screen presentation started before UpdateViewScreenState
  // is completed.
  ViewScreenRequestCallback update_view_screen_cb =
      update_view_screen_future.Take();
  EXPECT_CALL(*teacher_screen_presenter, IsPresenting).WillOnce(Return(true));
  std::move(update_view_screen_cb).Run(true);

  EXPECT_FALSE(success_future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest,
       PresentStudentScreenEndViewScreenFailure) {
  base::HistogramTester histogram_tester;
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  base::test::TestFuture<bool> success_future;
  ::boca::Session session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*session_manager(), EndSpotlightSession)
      .WillOnce([](base::OnceClosure callback) { std::move(callback).Run(); });
  EXPECT_CALL(*spotlight_service(), UpdateViewScreenState)
      .WillOnce([](std::string, ::boca::ViewScreenConfig::ViewScreenState,
                   std::string, ViewScreenRequestCallback callback) {
        std::move(callback).Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_BAD_REQUEST));
      });
  EXPECT_CALL(*student_screen_presenter, Start).Times(0);
  boca_app_handler()->PresentStudentScreen(
      mojom::Identity::New(kActiveStudentId, "student name",
                           "student@email.com", std::nullopt),
      kReceiverId, success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
  histogram_tester.ExpectTotalCount(
      kBocaPresentStudentScreenFailureReasonUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentStudentScreenFailureReasonUmaPath,
      /* kEndSpotlightFailed */ 4, 1);
  histogram_tester.ExpectTotalCount(kBocaPresentStudentScreenResultUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaPresentStudentScreenResultUmaPath,
                                     /* failure*/ 0, 1);
}

TEST_F(BocaAppPageHandlerProducerTest,
       PresentStudentScreenSessionInactiveAfterCall) {
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  ON_CALL(*session_manager(), GetStudentActiveDeviceId)
      .WillByDefault(Return(kStudentDeviceId));
  base::test::TestFuture<bool> success_future;
  ViewScreenRequestCallback view_screen_update_cb;
  ::boca::Session session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*session_manager(), EndSpotlightSession)
      .WillOnce([](base::OnceClosure callback) { std::move(callback).Run(); });
  EXPECT_CALL(*spotlight_service(), UpdateViewScreenState)
      .WillOnce([&view_screen_update_cb](
                    std::string, ::boca::ViewScreenConfig::ViewScreenState,
                    std::string, ViewScreenRequestCallback callback) {
        view_screen_update_cb = std::move(callback);
      });
  EXPECT_CALL(*student_screen_presenter, Start).Times(0);
  boca_app_handler()->PresentStudentScreen(
      mojom::Identity::New(kActiveStudentId, "student name",
                           "student@email.com", std::nullopt),
      kReceiverId, success_future.GetCallback());
  session.set_session_state(::boca::Session::PAST);
  std::move(view_screen_update_cb).Run(true);
  EXPECT_FALSE(success_future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest, PresentStudentScreenNoDeviceFound) {
  base::HistogramTester histogram_tester;
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  base::test::TestFuture<bool> success_future;
  ::boca::Session session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*session_manager(), EndSpotlightSession)
      .WillOnce([](base::OnceClosure callback) { std::move(callback).Run(); });
  EXPECT_CALL(*spotlight_service(), UpdateViewScreenState)
      .WillOnce([](std::string, ::boca::ViewScreenConfig::ViewScreenState,
                   std::string, ViewScreenRequestCallback callback) {
        std::move(callback).Run(true);
      });
  EXPECT_CALL(*session_manager(), GetStudentActiveDeviceId)
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*student_screen_presenter, Start).Times(0);
  boca_app_handler()->PresentStudentScreen(
      mojom::Identity::New(kActiveStudentId, "student name",
                           "student@email.com", std::nullopt),
      kReceiverId, success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
  histogram_tester.ExpectTotalCount(
      kBocaPresentStudentScreenFailureReasonUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentStudentScreenFailureReasonUmaPath,
      /* kNoActiveStudentDevice */ 5, 1);
  histogram_tester.ExpectTotalCount(kBocaPresentStudentScreenResultUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaPresentStudentScreenResultUmaPath,
                                     /* failure*/ 0, 1);
}

TEST_F(BocaAppPageHandlerProducerTest, PresentStudentScreenWhenNull) {
  base::HistogramTester histogram_tester;
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(nullptr));
  base::test::TestFuture<bool> success_future;
  ::boca::Session session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  boca_app_handler()->PresentStudentScreen(
      mojom::Identity::New(kActiveStudentId, "student name",
                           "student@email.com", std::nullopt),
      kReceiverId, success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
  histogram_tester.ExpectTotalCount(
      kBocaPresentStudentScreenFailureReasonUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentStudentScreenFailureReasonUmaPath,
      /* kFeatureDisabled */ 0, 1);
  histogram_tester.ExpectTotalCount(kBocaPresentStudentScreenResultUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaPresentStudentScreenResultUmaPath,
                                     /* failure*/ 0, 1);
}

TEST_F(BocaAppPageHandlerProducerTest, StopPresentingStudentScreenSuccess) {
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  base::test::TestFuture<bool> success_future;
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*student_screen_presenter, Stop)
      .WillOnce([](base::OnceCallback<void(bool)> success_cb) {
        std::move(success_cb).Run(true);
      });
  boca_app_handler()->StopPresentingStudentScreen(success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest, StopPresentingStudentScreenFailure) {
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  base::test::TestFuture<bool> success_future;
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*student_screen_presenter, Stop)
      .WillOnce([](base::OnceCallback<void(bool)> success_cb) {
        std::move(success_cb).Run(false);
      });
  boca_app_handler()->StopPresentingStudentScreen(success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest, PresentingStudentScreenOnSessionEnd) {
  base::test::TestFuture<void> disconnected_future;
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  EXPECT_CALL(*student_screen_presenter, IsPresenting).WillOnce(Return(true));
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*student_screen_presenter, Stop).Times(1);
  boca_app_handler()->OnSessionEnded("session_id");

  fake_page()->SetPresentStudentScreenEndedInterceptorCallback(
      disconnected_future.GetCallback());
  EXPECT_TRUE(disconnected_future.Wait());
}

TEST_F(BocaAppPageHandlerProducerTest, NotPresentingStudentScreenOnSessionEnd) {
  base::test::TestFuture<void> disconnected_future;
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  EXPECT_CALL(*student_screen_presenter, IsPresenting).WillOnce(Return(false));
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*student_screen_presenter, Stop).Times(0);
  boca_app_handler()->OnSessionEnded("session_id");

  fake_page()->SetPresentStudentScreenEndedInterceptorCallback(
      disconnected_future.GetCallback());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(disconnected_future.IsReady());
}

TEST_F(BocaAppPageHandlerProducerTest, StopPresentingStudentScreenWhenNull) {
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(nullptr));
  base::test::TestFuture<bool> success_future;
  boca_app_handler()->StopPresentingStudentScreen(success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest,
       CheckPresentingStudentScreenConnectionOnInvalidation) {
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  boca_app_handler()->OnSessionStarted("session_id", ::boca::UserIdentity());
  EXPECT_CALL(*student_screen_presenter, CheckConnection).Times(1);
  boca_app_handler()->OnReceiverInvalidation();
}

TEST_F(BocaAppPageHandlerProducerTest, PresentOwnScreenSuccess) {
  auto teacher_screen_presenter =
      std::make_unique<MockTeacherScreenPresenter>();
  ON_CALL(*session_manager(), GetTeacherScreenPresenter)
      .WillByDefault(Return(teacher_screen_presenter.get()));
  base::OnceClosure disconnected_callback;
  base::test::TestFuture<bool> success_future;
  base::test::TestFuture<void> disconnected_future;
  EXPECT_CALL(*teacher_screen_presenter,
              Start(kReceiverId, kReceiverName, _, _, _, _))
      .WillOnce([&disconnected_callback](
                    std::string_view, std::string_view, ::boca::UserIdentity,
                    bool, base::OnceCallback<void(bool)> success_cb,
                    base::OnceClosure disconnected_cb) {
        disconnected_callback = std::move(disconnected_cb);
        std::move(success_cb).Run(true);
      });
  boca_app_handler()->PresentOwnScreen(kReceiverId,
                                       success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());

  fake_page()->SetPresentOwnScreenEndedInterceptorCallback(
      disconnected_future.GetCallback());
  std::move(disconnected_callback).Run();
  EXPECT_TRUE(disconnected_future.Wait());
}

TEST_F(BocaAppPageHandlerProducerTest, PresentOwnScreenFail) {
  auto teacher_screen_presenter =
      std::make_unique<MockTeacherScreenPresenter>();
  ON_CALL(*session_manager(), GetTeacherScreenPresenter)
      .WillByDefault(Return(teacher_screen_presenter.get()));
  base::test::TestFuture<bool> success_future;
  EXPECT_CALL(*teacher_screen_presenter,
              Start(kReceiverId, kReceiverName, _, _, _, _))
      .WillOnce([](std::string_view, std::string_view, ::boca::UserIdentity,
                   bool, base::OnceCallback<void(bool)> success_cb,
                   base::OnceClosure) { std::move(success_cb).Run(false); });
  boca_app_handler()->PresentOwnScreen(kReceiverId,
                                       success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest,
       PresentOwnScreenWhilePresentingStudentScreen) {
  base::HistogramTester histogram_tester;
  auto teacher_screen_presenter =
      std::make_unique<MockTeacherScreenPresenter>();
  auto student_screen_presenter =
      std::make_unique<MockStudentScreenPresenter>();
  ON_CALL(*session_manager(), GetTeacherScreenPresenter)
      .WillByDefault(Return(teacher_screen_presenter.get()));
  ON_CALL(*session_manager(), GetStudentScreenPresenter)
      .WillByDefault(Return(student_screen_presenter.get()));
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  base::test::TestFuture<bool> success_future;
  EXPECT_CALL(*teacher_screen_presenter, Start).Times(0);
  EXPECT_CALL(*student_screen_presenter, IsPresenting).WillOnce(Return(true));
  boca_app_handler()->PresentOwnScreen(kReceiverId,
                                       success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
  histogram_tester.ExpectTotalCount(
      kBocaPresentOwnScreenInSessionFailureReasonUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentOwnScreenInSessionFailureReasonUmaPath,
      /* kStudentScreenShareActive */ 1, 1);
  histogram_tester.ExpectTotalCount(kBocaPresentOwnScreenInSessionResultUmaPath,
                                    1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentOwnScreenInSessionResultUmaPath, /* failure*/ 0, 1);
}

TEST_F(BocaAppPageHandlerProducerTest, StopPresentingOwnScreenSuccess) {
  auto teacher_screen_presenter =
      std::make_unique<MockTeacherScreenPresenter>();
  ON_CALL(*session_manager(), GetTeacherScreenPresenter)
      .WillByDefault(Return(teacher_screen_presenter.get()));
  base::test::TestFuture<bool> success_future;
  EXPECT_CALL(*teacher_screen_presenter, Start).Times(1);
  EXPECT_CALL(*teacher_screen_presenter, Stop)
      .WillOnce([](base::OnceCallback<void(bool)> success_cb) {
        std::move(success_cb).Run(true);
      });
  boca_app_handler()->PresentOwnScreen(kReceiverId, base::DoNothing());
  boca_app_handler()->StopPresentingOwnScreen(success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest, StopPresentingOwnScreenFailure) {
  auto teacher_screen_presenter =
      std::make_unique<MockTeacherScreenPresenter>();
  ON_CALL(*session_manager(), GetTeacherScreenPresenter)
      .WillByDefault(Return(teacher_screen_presenter.get()));
  base::test::TestFuture<bool> success_future;
  EXPECT_CALL(*teacher_screen_presenter, Start).Times(1);
  EXPECT_CALL(*teacher_screen_presenter, Stop)
      .WillOnce([](base::OnceCallback<void(bool)> success_cb) {
        std::move(success_cb).Run(false);
      });
  boca_app_handler()->PresentOwnScreen(kReceiverId, base::DoNothing());
  boca_app_handler()->StopPresentingOwnScreen(success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest, StopPresentingOwnScreenWhenNull) {
  ON_CALL(*session_manager(), GetTeacherScreenPresenter)
      .WillByDefault(Return(nullptr));
  base::test::TestFuture<bool> success_future;
  boca_app_handler()->StopPresentingOwnScreen(success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());
}

TEST_F(BocaAppPageHandlerProducerTest,
       PresentOwnScreenOutOfSessionFailureFeatureDisabled) {
  base::HistogramTester histogram_tester;
  // Mock that the TeacherScreenPresenter is nullptr, which happens when the
  // feature is disabled.
  ON_CALL(*session_manager(), GetTeacherScreenPresenter)
      .WillByDefault(Return(nullptr));
  base::test::TestFuture<bool> success_future;
  boca_app_handler()->PresentOwnScreen(kReceiverId,
                                       success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
  histogram_tester.ExpectTotalCount(
      kBocaPresentOwnScreenOutOfSessionFailureReasonUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentOwnScreenOutOfSessionFailureReasonUmaPath,
      /* kFeatureDisabled */ 0, 1);
  histogram_tester.ExpectTotalCount(
      kBocaPresentOwnScreenOutOfSessionResultUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentOwnScreenOutOfSessionResultUmaPath, /* failure*/ 0, 1);
}

TEST_F(BocaAppPageHandlerProducerTest,
       PresentOwnScreenInSessionFailureFeatureDisabled) {
  base::HistogramTester histogram_tester;
  // Mock that the TeacherScreenPresenter is nullptr, which happens when the
  // feature is disabled.
  ON_CALL(*session_manager(), GetTeacherScreenPresenter)
      .WillByDefault(Return(nullptr));
  base::test::TestFuture<bool> success_future;
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillRepeatedly(Return(&session));
  boca_app_handler()->PresentOwnScreen(kReceiverId,
                                       success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
  histogram_tester.ExpectTotalCount(
      kBocaPresentOwnScreenInSessionFailureReasonUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentOwnScreenInSessionFailureReasonUmaPath,
      /* kFeatureDisabled */ 0, 1);
  histogram_tester.ExpectTotalCount(kBocaPresentOwnScreenInSessionResultUmaPath,
                                    1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentOwnScreenInSessionResultUmaPath, /* failure*/ 0, 1);
}

TEST_F(BocaAppPageHandlerProducerTest, PresentStudentScreenFailureNoSession) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> success_future;
  boca_app_handler()->PresentStudentScreen(
      mojom::Identity::New(kActiveStudentId, "student name",
                           "student@email.com", std::nullopt),
      kReceiverId, success_future.GetCallback());
  EXPECT_FALSE(success_future.Get());
  histogram_tester.ExpectTotalCount(
      kBocaPresentStudentScreenFailureReasonUmaPath, 1);
  histogram_tester.ExpectBucketCount(
      kBocaPresentStudentScreenFailureReasonUmaPath,
      /* kNoSession */ 6, 1);
  histogram_tester.ExpectTotalCount(kBocaPresentStudentScreenResultUmaPath, 1);
  histogram_tester.ExpectBucketCount(kBocaPresentStudentScreenResultUmaPath,
                                     /* failure*/ 0, 1);
}

class BocaAppPageHandlerProducerMarkerModeTest : public AshTestBase {
 public:
  BocaAppPageHandlerProducerMarkerModeTest() = default;
  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    AnnotatorController* annotator_controller =
        ash::Shell::Get()->annotator_controller();
    annotator_controller->SetToolClient(&client_);
  }

  void TearDown() override { AshTestBase::TearDown(); }

 protected:
  AnnotationTray* annotator_tray() {
    return ash::Shell::GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->annotation_tray();
  }

 private:
  MockAnnotatorClient client_;
};

TEST_F(BocaAppPageHandlerProducerMarkerModeTest, EnableAndDisableMarkerMode) {
  ash::boca::util::EnableOrDisableMarkerMode(/*enable=*/true);
  EXPECT_TRUE(annotator_tray()->visible_preferred());

  ash::boca::util::EnableOrDisableMarkerMode(/*enable=*/false);
  EXPECT_FALSE(annotator_tray()->visible_preferred());
}

}  // namespace
}  // namespace ash::boca
