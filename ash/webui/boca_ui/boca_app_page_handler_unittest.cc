// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include <memory>

#include "ash/webui/boca_ui/mojom/boca.mojom-forward.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-shared.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
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
#include "google_apis/common/request_sender.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {
namespace {
constexpr char kGaiaId[] = "123";
constexpr char kUserEmail[] = "cat@gmail.com";

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
  students.push_back(mojom::Identity::New("1", "a", "a@gmail.com"));
  students.push_back(mojom::Identity::New("2", "b", "b@gmail.com"));

  const auto caption_config = mojom::CaptionConfig::New();
  caption_config->caption_enabled = true;
  caption_config->transcription_enabled = true;

  std::vector<mojom::ControlledTabPtr> tabs;
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New("google", ::GURL("http://google.com/"), "data/image"),
      /*=navigation_type*/ mojom::NavigationType::kOpen));
  tabs.push_back(mojom::ControlledTab::New(
      mojom::TabInfo::New("youtube", ::GURL("http://youtube.com/"),
                          "data/image"),
      /*=navigation_type*/ mojom::NavigationType::kBlock));
  const auto on_task_config =
      mojom::OnTaskConfig::New(/*=is_locked*/ true, std::move(tabs));

  const auto config =
      mojom::Config::New(session_duration, std::move(students),
                         on_task_config->Clone(), caption_config->Clone());
  // Page handler callback.
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<bool> future_1;

  CreateSessionRequest request(
      nullptr, kGaiaId, config->session_duration,
      ::boca::Session::SessionState::Session_SessionState_ACTIVE,
      future.GetCallback());
  EXPECT_CALL(*session_client_impl(), CreateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId, request->teacher_gaia_id());
            ASSERT_EQ(session_duration, request->duration());
            ASSERT_EQ(
                ::boca::Session::SessionState::Session_SessionState_ACTIVE,
                request->session_state());

            // Optional attribute.
            ASSERT_EQ(2u, request->student_groups().size());
            EXPECT_EQ("1", request->student_groups()[0].gaia_id());
            EXPECT_EQ("a", request->student_groups()[0].full_name());
            EXPECT_EQ("a@gmail.com", request->student_groups()[0].email());

            EXPECT_EQ("2", request->student_groups()[1].gaia_id());
            EXPECT_EQ("b", request->student_groups()[1].full_name());
            EXPECT_EQ("b@gmail.com", request->student_groups()[1].email());

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
            request->callback().Run("success");
          })));

  // Verify local events dispatched
  EXPECT_CALL(*boca_app_client(), GetSessionManager())
      .WillOnce(Return(session_manager()));
  EXPECT_CALL(*session_manager(), NotifyLocalCaptionEvents(_)).Times(1);

  boca_app_handler_->CreateSession(config->Clone(), future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get());
}

TEST_F(BocaAppPageHandlerTest, CreateSessionWithCritialInputOnly) {
  auto session_duration = base::Minutes(2);

  // Page handler callback.
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  // API callback.
  base::test::TestFuture<bool> future_1;

  const auto config = mojom::Config::New(
      session_duration, std::vector<mojom::IdentityPtr>{},
      mojom::OnTaskConfigPtr(nullptr), mojom::CaptionConfigPtr(nullptr));

  CreateSessionRequest request(
      nullptr, kGaiaId, config->session_duration,
      ::boca::Session::SessionState::Session_SessionState_ACTIVE,
      future.GetCallback());
  EXPECT_CALL(*session_client_impl(), CreateSession(_))
      .WillOnce(WithArg<0>(
          // Unique pointer have ownership issue, have to do manual deep copy
          // here instead of using SaveArg.
          Invoke([&](auto request) {
            ASSERT_EQ(kGaiaId, request->teacher_gaia_id());
            ASSERT_EQ(session_duration, request->duration());
            ASSERT_EQ(
                ::boca::Session::SessionState::Session_SessionState_ACTIVE,
                request->session_state());

            ASSERT_FALSE(request->captions_config());
            ASSERT_FALSE(request->on_task_config());
            ASSERT_TRUE(request->student_groups().empty());

            request->callback().Run("success");
          })));

  // Verify local events not dispatched
  EXPECT_CALL(*boca_app_client(), GetSessionManager()).Times(0);
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
  base::test::TestFuture<mojom::ConfigPtr> future_1;

  GetSessionRequest request(nullptr, kGaiaId, future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        auto session = std::make_unique<::boca::Session>();
        session->mutable_duration()->set_seconds(120);

        auto* student_groups_1 =
            session->mutable_roster()->mutable_student_groups()->Add();
        student_groups_1->set_title(kMainStudentGroupName);
        auto* student = student_groups_1->mutable_students()->Add();
        student->set_email("dog@email.com");
        student->set_full_name("dog");
        student->set_gaia_id("123");

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

  boca_app_handler_->GetSession(future_1.GetCallback());

  auto result = future_1.Take();

  EXPECT_EQ(120, result->session_duration.InSeconds());

  EXPECT_EQ(true, result->caption_config->caption_enabled);
  EXPECT_EQ(true, result->caption_config->transcription_enabled);

  ASSERT_EQ(1u, result->students.size());

  EXPECT_EQ("dog", result->students[0]->name);
  EXPECT_EQ("123", result->students[0]->id);
  EXPECT_EQ("dog@email.com", result->students[0]->email);

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
  base::test::TestFuture<mojom::ConfigPtr> future_1;

  GetSessionRequest request(nullptr, kGaiaId, future.GetCallback());
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(WithArg<0>(Invoke([&](auto request) {
        auto session = std::make_unique<::boca::Session>();
        session->mutable_duration()->set_seconds(120);
        request->callback().Run(std::move(session));
      })));

  boca_app_handler_->GetSession(future_1.GetCallback());

  auto result = future_1.Take();
  EXPECT_EQ(120, result->session_duration.InSeconds());
}

}  // namespace
}  // namespace ash::boca
