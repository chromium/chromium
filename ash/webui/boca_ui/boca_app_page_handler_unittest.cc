// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include <memory>
#include <optional>

#include "ash/webui/boca_ui/mojom/boca.mojom-forward.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-shared.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/create_session_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {
namespace {
constexpr char kGaiaId[] = "123";
constexpr char kUserEmail[] = "cat@gmail.com";

mojom::OnTaskConfigPtr GetCommonTestLockOnTaskConfig() {
  std::vector<mojom::ControlledTabPtr> tabs;
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New("google", ::GURL("http://google.com/"), "data/image"),
      /*=navigation_type*/ mojom::NavigationType::kOpen));
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New("youtube", ::GURL("http://youtube.com/"),
                          "data/image"),
      /*=navigation_type*/ mojom::NavigationType::kBlock));
  return mojom::OnTaskConfig::New(/*=is_locked*/ true, std::move(tabs));
}

mojom::OnTaskConfigPtr GetCommonTestUnLockedOnTaskConfig() {
  std::vector<mojom::ControlledTabPtr> tabs;
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New("google", ::GURL("http://google.com/"), "data/image"),
      /*=navigation_type*/ mojom::NavigationType::kOpen));
  return mojom::OnTaskConfig::New(/*=is_locked*/ false, std::move(tabs));
}

::boca::OnTaskConfig GetCommonTestLockOnTaskConfigProto() {
  ::boca::OnTaskConfig on_task_config;
  auto* active_bundle = on_task_config.mutable_active_bundle();
  active_bundle->set_locked(true);
  auto* content = active_bundle->mutable_content_configs()->Add();
  content->set_url("http://google.com/");
  content->set_favicon_url("data/image");
  content->set_title("google");
  content->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);
  auto* content_1 = active_bundle->mutable_content_configs()->Add();
  content_1->set_url("http://youtube.com/");
  content_1->set_favicon_url("data/image");
  content_1->set_title("youtube");
  content_1->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType_BLOCK_NAVIGATION);
  return on_task_config;
}

::boca::OnTaskConfig GetCommonTestUnLockOnTaskConfigProto() {
  ::boca::OnTaskConfig on_task_config;
  auto* active_bundle = on_task_config.mutable_active_bundle();
  active_bundle->set_locked(false);
  auto* content = active_bundle->mutable_content_configs()->Add();
  content->set_url("http://google.com/");
  content->set_favicon_url("data/image");
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

::boca::Session GetCommonActiveSessionProto() {
  ::boca::Session session;
  session.mutable_duration()->set_seconds(120);
  session.set_session_state(::boca::Session::ACTIVE);
  auto* teacher = session.mutable_teacher();
  teacher->set_gaia_id("123");

  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(false);
  caption_config_1->set_translations_enabled(false);

  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(false);
  auto* content = active_bundle->mutable_content_configs()->Add();
  content->set_url("http://default.com/");
  content->set_favicon_url("data/image");
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
              (std::unique_ptr<GetSessionRequest>),
              (override));
  MOCK_METHOD(void,
              UpdateSession,
              (std::unique_ptr<UpdateSessionRequest>),
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
};

class MockSessionManager : public BocaSessionManager {
 public:
  explicit MockSessionManager(SessionClientImpl* session_client_impl)
      : BocaSessionManager(session_client_impl,
                           AccountId::FromUserEmail(kUserEmail)) {}
  MOCK_METHOD(void,
              NotifyLocalCaptionEvents,
              (::boca::CaptionsConfig config),
              (override));
  MOCK_METHOD(void,
              UpdateCurrentSession,
              (std::unique_ptr<::boca::Session>, bool),
              (override));
  MOCK_METHOD((::boca::Session*), GetCurrentSession, (), (override));
  ~MockSessionManager() override = default;
};

class BocaAppPageHandlerTest : public testing::Test {
 public:
  BocaAppPageHandlerTest() = default;
  void SetUp() override {
    // Sign in test user.
    auto account_id = AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaId);
    fake_user_manager_.Reset(std::make_unique<user_manager::FakeUserManager>());
    fake_user_manager_->AddUser(account_id);

    auto session_client_impl =
        std::make_unique<StrictMock<MockSessionClientImpl>>(nullptr);
    session_client_impl_ = session_client_impl.get();

    boca_app_client_ = std::make_unique<StrictMock<MockBocaAppClient>>();
    EXPECT_CALL(*boca_app_client_, AddSessionManager(_)).Times(1);

    session_manager_ =
        std::make_unique<StrictMock<MockSessionManager>>(session_client_impl_);

    boca_app_handler_ = std::make_unique<BocaAppHandler>(
        nullptr, remote_.BindNewPipeAndPassReceiver(),
        // TODO(b/359929870):Setting nullptr for other dependencies for now.
        // Adding test case for classroom and tab info.
        receiver_.InitWithNewPipeAndPassRemote(), nullptr, nullptr,
        std::move(session_client_impl));
  }

 protected:
  MockSessionClientImpl* session_client_impl() { return session_client_impl_; }
  MockBocaAppClient* boca_app_client() { return boca_app_client_.get(); }
  MockSessionManager* session_manager() { return session_manager_.get(); }

  std::unique_ptr<BocaAppHandler> boca_app_handler_;

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::PageHandler> remote_;
  mojo::PendingReceiver<mojom::Page> receiver_;
  raw_ptr<StrictMock<MockSessionClientImpl>> session_client_impl_;
  std::unique_ptr<StrictMock<MockBocaAppClient>> boca_app_client_;
  std::unique_ptr<StrictMock<MockSessionManager>> session_manager_;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(BocaAppPageHandlerTest, CreateSessionWithFullInput) {
  auto session_duration = base::Minutes(2);

  std::vector<mojom::IdentityPtr> students;
  students.push_back(
      mojom::Identity::New("1", "a", "a@gmail.com", GURL("cdn://s1")));
  students.push_back(
      mojom::Identity::New("2", "b", "b@gmail.com", GURL("cdn://s2")));

  const auto config = mojom::Config::New(
      session_duration, std::nullopt, nullptr, std::move(students),
      GetCommonTestLockOnTaskConfig(), GetCommonCaptionConfig());
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<bool> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId);
  CreateSessionRequest request(
      nullptr, teacher, config->session_duration,
      ::boca::Session::SessionState::Session_SessionState_ACTIVE,
      future.GetCallback());
  EXPECT_CALL(*session_client_impl(), CreateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId, request->teacher().gaia_id());
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
            EXPECT_EQ("data/image", request->on_task_config()
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
            EXPECT_EQ("data/image", request->on_task_config()
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
          })));

  // Verify local events dispatched
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(2)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  EXPECT_CALL(*session_manager(), UpdateCurrentSession(_, true)).Times(1);

  boca_app_handler_->CreateSession(config->Clone(), future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get());
}

TEST_F(BocaAppPageHandlerTest, CreateSessionWithCritialInputOnly) {
  auto session_duration = base::Minutes(2);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<bool> future_1;

  const auto config = mojom::Config::New(
      session_duration, std::nullopt, nullptr,
      std::vector<mojom::IdentityPtr>{}, mojom::OnTaskConfigPtr(nullptr),
      mojom::CaptionConfigPtr(nullptr));

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId);
  CreateSessionRequest request(
      nullptr, teacher, config->session_duration,
      ::boca::Session::SessionState::Session_SessionState_ACTIVE,
      future.GetCallback());
  EXPECT_CALL(*session_client_impl(), CreateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId, request->teacher().gaia_id());
            ASSERT_EQ(session_duration, request->duration());
            ASSERT_EQ(
                ::boca::Session::SessionState::Session_SessionState_ACTIVE,
                request->session_state());

            ASSERT_FALSE(request->captions_config());
            ASSERT_FALSE(request->on_task_config());
            ASSERT_FALSE(request->roster());
            request->callback().Run(std::make_unique<::boca::Session>());
          })));

  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession(_, true)).Times(1);

  // Verify local events not dispatched
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(0);

  boca_app_handler_->CreateSession(config.Clone(), future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get());
}

TEST_F(BocaAppPageHandlerTest, GetSessionWithFullInputTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kGaiaId, future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
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

        auto* student_groups_1 =
            session->mutable_roster()->mutable_student_groups()->Add();
        student_groups_1->set_title(kMainStudentGroupName);
        auto* student = student_groups_1->mutable_students()->Add();
        student->set_email("dog@email.com");
        student->set_full_name("dog");
        student->set_gaia_id("123");
        student->set_photo_url("cdn://s1");

        ::boca::SessionConfig session_config;
        auto* caption_config_1 = session_config.mutable_captions_config();

        caption_config_1->set_captions_enabled(true);
        caption_config_1->set_translations_enabled(true);

        auto* active_bundle =
            session_config.mutable_on_task_config()->mutable_active_bundle();
        active_bundle->set_locked(true);
        auto* content = active_bundle->mutable_content_configs()->Add();
        content->set_url("http://google.com/");
        content->set_favicon_url("data/image");
        content->set_title("google");
        content->mutable_locked_navigation_options()->set_navigation_type(
            ::boca::LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);

        (*session->mutable_student_group_configs())[kMainStudentGroupName] =
            std::move(session_config);
        request->callback().Run(std::move(session));
      })));

  EXPECT_CALL(*session_manager(), UpdateCurrentSession(NotNull(), false))
      .Times(1);
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));

  boca_app_handler_->GetSession(future_1.GetCallback());

  auto result = std::move(future_1.Take()->get_config());

  EXPECT_EQ(120, result->session_duration.InSeconds());
  EXPECT_EQ(1111111.022,
            result->session_start_time->InSecondsFSinceUnixEpoch());
  EXPECT_EQ("teacher", result->teacher->name);
  EXPECT_EQ("teacher@email.com", result->teacher->email);
  EXPECT_EQ("000", result->teacher->id);
  EXPECT_EQ("cdn://s", result->teacher->photo_url->spec());

  EXPECT_EQ(true, result->caption_config->session_caption_enabled);
  EXPECT_EQ(true, result->caption_config->session_translation_enabled);

  ASSERT_EQ(1u, result->students.size());

  EXPECT_EQ("dog", result->students[0]->name);
  EXPECT_EQ("123", result->students[0]->id);
  EXPECT_EQ("dog@email.com", result->students[0]->email);
  EXPECT_EQ("cdn://s1", result->students[0]->photo_url->spec());

  ASSERT_EQ(1u, result->on_task_config->tabs.size());
  ASSERT_TRUE(result->on_task_config->is_locked);
  EXPECT_EQ(mojom::NavigationType::kOpen,
            result->on_task_config->tabs[0]->navigation_type);
  EXPECT_EQ("http://google.com/",
            result->on_task_config->tabs[0]->tab->url.spec());
  EXPECT_EQ("google", result->on_task_config->tabs[0]->tab->title);
  EXPECT_EQ("data/image", result->on_task_config->tabs[0]->tab->favicon);
}

TEST_F(BocaAppPageHandlerTest, GetSessionWithPartialInputTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;
  GetSessionRequest request(nullptr, kGaiaId, future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        auto session = std::make_unique<::boca::Session>();
        session->mutable_duration()->set_seconds(120);
        session->set_session_state(::boca::Session::ACTIVE);
        request->callback().Run(std::move(session));
      })));

  EXPECT_CALL(*session_manager(), UpdateCurrentSession(NotNull(), false))
      .Times(1);
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));

  boca_app_handler_->GetSession(future_1.GetCallback());

  auto result = std::move(future_1.Take()->get_config());
  EXPECT_EQ(120, result->session_duration.InSeconds());
}

TEST_F(BocaAppPageHandlerTest, GetSessionWithHTTPError) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kGaiaId, future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        request->callback().Run(
            base::unexpected(google_apis::ApiErrorCode::HTTP_BAD_REQUEST));
      })));

  EXPECT_CALL(*session_manager(), UpdateCurrentSession(_, false)).Times(0);
  EXPECT_CALL(*boca_app_client(), GetSessionManager()).Times(0);
  boca_app_handler_->GetSession(future_1.GetCallback());
  auto result = future_1.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(mojom::GetSessionError::kHTTPError, result->get_error());
}

TEST_F(BocaAppPageHandlerTest, GetSessionWithNullPtrInputTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kGaiaId, future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke(
          [&](auto request) { request->callback().Run(base::ok(nullptr)); })));

  EXPECT_CALL(*session_manager(), UpdateCurrentSession(IsNull(), false))
      .Times(1);
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));
  boca_app_handler_->GetSession(future_1.GetCallback());

  auto result = future_1.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(mojom::GetSessionError::kEmpty, result->get_error());
}

TEST_F(BocaAppPageHandlerTest, GetSessionWithNonActiveSessionTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;
  GetSessionRequest request(nullptr, kGaiaId, future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        request->callback().Run(std::make_unique<::boca::Session>());
      })));

  EXPECT_CALL(*session_manager(), UpdateCurrentSession(IsNull(), false))
      .Times(1);
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));
  boca_app_handler_->GetSession(future_1.GetCallback());
  auto result = future_1.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(mojom::GetSessionError::kEmpty, result->get_error());
}

TEST_F(BocaAppPageHandlerTest,
       GetSessionWithEmptySessionConfigShouldNotCrashTest) {
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<mojom::SessionResultPtr> future_1;

  GetSessionRequest request(nullptr, kGaiaId, future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        auto session = std::make_unique<::boca::Session>();
        session->set_session_state(::boca::Session::ACTIVE);
        request->callback().Run(std::move(session));
      })));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession(NotNull(), false))
      .Times(1);
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));

  boca_app_handler_->GetSession(future_1.GetCallback());
  auto result = future_1.Take();
  ASSERT_FALSE(result->is_error());
}

TEST_F(BocaAppPageHandlerTest, EndSessionSucceed) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->mutable_duration()->set_seconds(120);
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(2)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession(_, true)).Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId);
  UpdateSessionRequest request(nullptr, teacher, session_id,
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId, request->teacher().gaia_id());
            ASSERT_EQ(::boca::Session::PAST, *request->session_state());
            request->callback().Run(std::make_unique<::boca::Session>());
          })));

  boca_app_handler_->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, EndSessionWithHTTPFailure) {
  auto* session_id = "123";
  auto session = std::make_unique<::boca::Session>();
  session->mutable_duration()->set_seconds(120);
  session->set_session_state(::boca::Session::ACTIVE);

  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(session.get()));
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id(kGaiaId);
  UpdateSessionRequest request(nullptr, teacher, session_id,
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId, request->teacher().gaia_id());
            ASSERT_EQ(::boca::Session::PAST, *request->session_state());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          })));

  boca_app_handler_->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, EndSessionWithEmptyResponse) {
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler_->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, EndSessionWithNonActiveResponse) {
  ::boca::Session session;
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler_->EndSession(future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, UpdateOnTaskConfigSucceed) {
  auto session = GetCommonActiveSessionProto();

  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(2)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession(_, true)).Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, session.teacher(), session.session_id(),
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
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
          })));
  boca_app_handler_->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                        future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, UpdateOnTaskConfigWithEmptySession) {
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler_->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                        future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, UpdateOnTaskConfigWithNonActiveSession) {
  ::boca::Session non_active_session;
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&non_active_session));

  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler_->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                        future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, UpdateOnTaskConfigWithHTTPFailure) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(2)
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(2)
      .WillRepeatedly(Return(session_manager()));

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, session.teacher(), session.session_id(),
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(session.teacher().gaia_id(),
                      request->teacher().gaia_id());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          })));

  boca_app_handler_->UpdateOnTaskConfig(GetCommonTestLockOnTaskConfig(),
                                        future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kHTTPError, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, UpdateCaptionWithEmptySession) {
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(2)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler_->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, UpdateCaptionWithNonActiveSession) {
  ::boca::Session session;
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(2)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  boca_app_handler_->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kInvalid, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest, UpdateCaptionConfigSucceed) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(3)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession(_, true)).Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .WillOnce(Return(&session));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);

  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, session.teacher(), session.session_id(),
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
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
          })));

  boca_app_handler_->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest, UpdateCaptionWithHTTPFailure) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(2)
      .WillRepeatedly(Return(&session));
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(3)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);
  // Page handler callback.
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<std::optional<mojom::UpdateSessionError>> future_1;

  UpdateSessionRequest request(nullptr, session.teacher(), session.session_id(),
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(session.teacher().gaia_id(),
                      request->teacher().gaia_id());
            request->callback().Run(
                base::unexpected(google_apis::ApiErrorCode::HTTP_FORBIDDEN));
          })));

  boca_app_handler_->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                         future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_EQ(mojom::UpdateSessionError::kHTTPError, future_1.Get().value());
}

TEST_F(BocaAppPageHandlerTest,
       UpdateOnTaskConfigWithPendingCaptionConfigShouldNotOverride) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(5)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession(_, true)).Times(2);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(2)
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

  UpdateSessionRequest request(nullptr, session.teacher(), session.session_id(),
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        // Use latest on task cofig value from session.
        ASSERT_EQ(GetCommonActiveSessionProto()
                      .student_group_configs()
                      .at(kMainStudentGroupName)
                      .on_task_config()
                      .SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        request->callback().Run(std::unique_ptr<::boca::Session>());
      })))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        // Use pending on task config.
        ASSERT_EQ(GetCommonTestUnLockOnTaskConfigProto().SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        request->callback().Run(std::unique_ptr<::boca::Session>());
      })));
  boca_app_handler_->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                         future_1.GetCallback());
  boca_app_handler_->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                        future_2.GetCallback());

  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());

  ASSERT_TRUE(future_2.Wait());
  EXPECT_FALSE(future_2.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest,
       UpdateCaptionConfigWithPendingOnTaskConfigShouldNotOverride) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(5)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession(_, true)).Times(2);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(2)
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

  UpdateSessionRequest request(nullptr, session.teacher(), session.session_id(),
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
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
        request->callback().Run(std::unique_ptr<::boca::Session>());
      })))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        ASSERT_EQ(session.teacher().gaia_id(), request->teacher().gaia_id());
        ASSERT_EQ(GetCommonTestUnLockOnTaskConfigProto().SerializeAsString(),
                  request->on_task_config()->SerializeAsString());
        // Use pending on task config.
        ASSERT_EQ(GetCommonCaptionConfigProto().SerializeAsString(),
                  request->captions_config()->SerializeAsString());
        request->callback().Run(std::unique_ptr<::boca::Session>());
      })));
  boca_app_handler_->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                        future_1.GetCallback());
  boca_app_handler_->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                         future_2.GetCallback());

  ASSERT_TRUE(future_1.Wait());
  EXPECT_FALSE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Wait());
  EXPECT_FALSE(future_2.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest,
       UpdateOnTaskConfigWithFailedCaptionConfigShouldUseSessionData) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(5)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession(_, true)).Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(3)
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

  UpdateSessionRequest request(nullptr, session.teacher(), session.session_id(),
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
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
      })))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
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
      })));
  boca_app_handler_->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                         future_1.GetCallback());
  boca_app_handler_->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                        future_2.GetCallback());

  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Wait());
  EXPECT_FALSE(future_2.Get().has_value());
}

TEST_F(BocaAppPageHandlerTest,
       UpdateCaptionConfigWithFailedOnTaskConfigShouldUseSessionData) {
  auto session = GetCommonActiveSessionProto();
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .Times(5)
      .WillRepeatedly(Return(session_manager()));
  EXPECT_CALL(*session_manager(), UpdateCurrentSession(_, true)).Times(1);
  EXPECT_CALL(*session_manager(), GetCurrentSession())
      .Times(3)
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

  UpdateSessionRequest request(nullptr, session.teacher(), session.session_id(),
                               future.GetCallback());

  EXPECT_CALL(*session_client_impl(), UpdateSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
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
      })))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
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
      })));
  boca_app_handler_->UpdateOnTaskConfig(GetCommonTestUnLockedOnTaskConfig(),
                                        future_1.GetCallback());
  boca_app_handler_->UpdateCaptionConfig(GetCommonCaptionConfig(),
                                         future_2.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Wait());
  EXPECT_FALSE(future_2.Get().has_value());
}
}  // namespace
}  // namespace ash::boca
