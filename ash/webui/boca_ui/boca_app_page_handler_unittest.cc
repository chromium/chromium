// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-forward.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-shared.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/create_session_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "components/account_id/account_id.h"
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
};

class BocaAppPageHandlerTest : public AshTestBase {
 public:
  BocaAppPageHandlerTest() = default;
  void SetUp() override {
    auto session_client_impl =
        std::make_unique<StrictMock<MockSessionClientImpl>>(nullptr);
    session_client_impl_ = session_client_impl.get();

    // Ash test base setup.
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    // Sign in test user.
    AshTestBase::SimulateUserLogin(
        AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaId));
    boca_app_handler_ = std::make_unique<BocaAppHandler>(
        nullptr, remote_.BindNewPipeAndPassReceiver(),
        // TODO(b/359929870):Setting nullptr for other dependencies for now.
        // Adding test case for classroom and tab info.
        receiver_.InitWithNewPipeAndPassRemote(), nullptr, nullptr,
        std::move(session_client_impl));
  }

 protected:
  MockSessionClientImpl* session_client_impl() { return session_client_impl_; }
  std::unique_ptr<BocaAppHandler> boca_app_handler_;

 private:
  mojo::Remote<mojom::PageHandler> remote_;
  mojo::PendingReceiver<mojom::Page> receiver_;
  raw_ptr<StrictMock<MockSessionClientImpl>> session_client_impl_;
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

  boca_app_handler_->CreateSession(config.Clone(), future_1.GetCallback());
  ASSERT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_1.Get());
}
}  // namespace ash::boca
