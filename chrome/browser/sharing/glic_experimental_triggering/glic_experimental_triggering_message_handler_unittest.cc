// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_instance_coordinator.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sharing_message/mock_sharing_message_sender.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

class MockGlicKeyedService : public glic::GlicKeyedService {
 public:
  MockGlicKeyedService(Profile* profile,
                       signin::IdentityManager* identity_manager,
                       ProfileManager* profile_manager,
                       glic::GlicProfileManager* glic_profile_manager)
      : glic::GlicKeyedService(profile,
                               identity_manager,
                               profile_manager,
                               glic_profile_manager,
                               nullptr,
                               nullptr) {}
  ~MockGlicKeyedService() override = default;

  MOCK_METHOD(void,
              InvokeWithAutoSubmit,
              (glic::InvokeWithAutoSubmitPasskey, glic::GlicInvokeOptions),
              (override));
  MOCK_METHOD(glic::GlicInstanceCoordinator&,
              instance_coordinator,
              (),
              (const, override));
};

class TestGlicExperimentalTriggeringMessageHandler
    : public GlicExperimentalTriggeringMessageHandler {
 public:
  TestGlicExperimentalTriggeringMessageHandler(
      Profile* profile,
      SharingMessageSender* message_sender)
      : GlicExperimentalTriggeringMessageHandler(profile, message_sender) {}
  ~TestGlicExperimentalTriggeringMessageHandler() override = default;

  void SetActiveTab(tabs::TabInterface* tab) { active_tab_ = tab; }

 protected:
  tabs::TabInterface* GetActiveTab() const override { return active_tab_; }

 private:
  raw_ptr<tabs::TabInterface> active_tab_ = nullptr;
};

class GlicExperimentalTriggeringMessageHandlerTest : public testing::Test {
 public:
  GlicExperimentalTriggeringMessageHandlerTest() = default;
  ~GlicExperimentalTriggeringMessageHandlerTest() override = default;

 protected:
  void SetUp() override {
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

    TestingProfileManager* testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
    profile_ = testing_profile_manager->CreateTestingProfile("test_profile");

    glic::GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile_,
        base::BindRepeating(
            [](Profile* p, signin::IdentityManager* im, ProfileManager* pm,
               glic::GlicProfileManager* gpm, content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              return std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
                  p, im, pm, gpm);
            },
            profile_, identity_test_environment_.identity_manager(),
            testing_profile_manager->profile_manager(),
            &glic_profile_manager_));
    mock_glic_service_ = static_cast<MockGlicKeyedService*>(
        glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_,
                                                           /*create=*/true));
    ASSERT_TRUE(mock_glic_service_);
    EXPECT_CALL(*mock_glic_service_, instance_coordinator())
        .WillRepeatedly(testing::ReturnRef(mock_instance_coordinator_));

    handler_ = std::make_unique<TestGlicExperimentalTriggeringMessageHandler>(
        profile_, &mock_sharing_message_sender_);
  }

  void TearDown() override {
    handler_.reset();
    mock_glic_service_ = nullptr;
    profile_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  content::BrowserTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  base::test::ScopedFeatureList feature_list_{
      features::kGlicExperimentalTriggering};
  raw_ptr<TestingProfile> profile_ = nullptr;
  testing::NiceMock<MockSharingMessageSender> mock_sharing_message_sender_;
  std::unique_ptr<TestGlicExperimentalTriggeringMessageHandler> handler_;
  glic::MockGlicInstanceCoordinator mock_instance_coordinator_;
  raw_ptr<MockGlicKeyedService> mock_glic_service_ = nullptr;
  glic::GlicProfileManager glic_profile_manager_;
};

TEST_F(GlicExperimentalTriggeringMessageHandlerTest,
       HandlesMessageGracefullyWhenNoActiveTab) {
  components_sharing_message::SharingMessage message;
  message.mutable_glic_experimental_triggering();

  base::MockOnceCallback<void(
      std::unique_ptr<components_sharing_message::ResponseMessage>)>
      done_callback;

  EXPECT_CALL(done_callback, Run(_)).Times(1);

  handler_->OnMessage(std::move(message), done_callback.Get());
}

TEST_F(GlicExperimentalTriggeringMessageHandlerTest, RelaysUpdatesToServer) {
  components_sharing_message::SharingMessage message;
  message.mutable_glic_experimental_triggering();
  auto* server_channel_config = message.mutable_server_channel_configuration();
  server_channel_config->set_configuration("test_config");

  base::MockOnceCallback<void(
      std::unique_ptr<components_sharing_message::ResponseMessage>)>
      done_callback;

  tabs::MockTabInterface mock_tab;
  handler_->SetActiveTab(&mock_tab);

  // Expect InvokeWithAutoSubmit to be called
  EXPECT_CALL(*mock_glic_service_, InvokeWithAutoSubmit(_, _)).Times(1);

  // Expect GetExperimentalTriggeringUpdates to be called
  mojo::PendingRemote<glic::mojom::ExperimentalTriggeringUpdatesHandler>
      updates_handler_remote;
  EXPECT_CALL(mock_instance_coordinator_,
              GetExperimentalTriggeringUpdates(_, _))
      .WillOnce(
          [&updates_handler_remote](
              mojo::PendingRemote<
                  glic::mojom::ExperimentalTriggeringUpdatesHandler> remote,
              base::OnceCallback<void(bool)> callback) {
            updates_handler_remote = std::move(remote);
            std::move(callback).Run(true);
          });

  EXPECT_CALL(done_callback, Run(_)).Times(1);

  handler_->OnMessage(std::move(message), done_callback.Get());

  // Now simulate an update from the remote
  ASSERT_TRUE(updates_handler_remote.is_valid());
  mojo::Remote<glic::mojom::ExperimentalTriggeringUpdatesHandler>
      updates_handler(std::move(updates_handler_remote));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_sharing_message_sender_,
              SendMessageToServerTarget(_, _, _, _, _))
      .WillOnce(
          [quit_closure = run_loop.QuitClosure()](
              const components_sharing_message::ServerChannelConfiguration&
                  server_channel,
              base::TimeDelta timeout,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::DelegateType delegate_type,
              SharingMessageSender::ResponseCallback callback) {
            EXPECT_EQ(server_channel.configuration(), "test_config");
            EXPECT_EQ(timeout, base::Seconds(10));
            EXPECT_TRUE(message.has_glic_experimental_triggering());
            EXPECT_TRUE(message.glic_experimental_triggering().has_response());
            EXPECT_TRUE(message.glic_experimental_triggering()
                            .response()
                            .has_task_update());
            EXPECT_EQ(message.glic_experimental_triggering()
                          .response()
                          .task_update()
                          .data(),
                      "test_update");
            quit_closure.Run();
            return base::OnceClosure();
          });

  // Trigger the update
  glic::mojom::ExperimentalTriggeringUpdatePtr update =
      glic::mojom::ExperimentalTriggeringUpdate::New();
  update->type = glic::mojom::ExperimentalTriggeringUpdateType::kWorklog;
  update->data = "test_update";
  updates_handler->OnUpdate(std::move(update),
                            glic::mojom::SubscriberObservationType::kUpdate);

  // Wait for Mojo tasks to run
  run_loop.Run();
}

}  // namespace
