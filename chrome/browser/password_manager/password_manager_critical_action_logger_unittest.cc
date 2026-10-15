// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_critical_action_logger.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

using ::testing::_;
using ::testing::SaveArg;

class MockCriticalActionService
    : public critical_actions::CriticalActionService {
 public:
  MockCriticalActionService(
      const base::FilePath& db_path,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
      : critical_actions::CriticalActionService(db_path, backend_task_runner) {}
  ~MockCriticalActionService() override = default;

  MOCK_METHOD(void,
              AddCriticalAction,
              (const critical_actions::CriticalActionEntry& entry),
              (override));
  MOCK_METHOD(void,
              AddCriticalActionWithNavigationId,
              (const critical_actions::CriticalActionEntry& entry,
               int64_t navigation_id),
              (override));
  MOCK_METHOD(void, OnNavigationDiscarded, (int64_t navigation_id), (override));
};

}  // namespace

class PasswordManagerCriticalActionLoggerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordManagerCriticalActionLoggerTest() {
    feature_list_.InitAndEnableFeature(
        critical_actions::features::kCriticalActionHistory);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_path =
        temp_dir_.GetPath().AppendASCII("TestCriticalActionsLogger.db");
    auto mock_service = std::make_unique<MockCriticalActionService>(
        db_path,
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
    mock_service_ = mock_service.get();

    critical_actions::CriticalActionFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindOnce(
            [](std::unique_ptr<MockCriticalActionService> service,
               content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> { return std::move(service); },
            std::move(mock_service)));

    actor::ActorKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<actor::ActorKeyedServiceFake>(
              Profile::FromBrowserContext(context));
        }));

    ON_CALL(mock_tab_, GetProfile).WillByDefault(testing::Return(profile()));
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &mock_tab_);

    // ContentPasswordManagerDriver::GetForRenderFrameHost requires the factory
    // to be registered.
    stub_client_ =
        std::make_unique<password_manager::StubPasswordManagerClient>();
    password_manager::ContentPasswordManagerDriverFactory::CreateForWebContents(
        web_contents(), stub_client_.get());

    logger_ =
        std::make_unique<password_manager::PasswordManagerCriticalActionLogger>(
            web_contents(), profile());

    nav_id_ = CommitNavigation(web_contents());
  }

  void TearDown() override {
    logger_.reset();
    mock_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
    stub_client_.reset();
  }

  MockCriticalActionService* mock_service() { return mock_service_; }

  password_manager::PasswordManagerCriticalActionLogger* logger() {
    return logger_.get();
  }

  tabs::MockTabInterface* mock_tab() { return &mock_tab_; }

  int64_t nav_id() const { return nav_id_; }

  int64_t CommitNavigation(content::WebContents* contents) {
    auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
        test_url_, contents);
    simulator->Start();
    int64_t nav_id = simulator->GetNavigationHandle()->GetNavigationId();
    simulator->Commit();
    return nav_id;
  }

  actor::TaskId StartActorTask() {
    auto* actor_service = static_cast<actor::ActorKeyedServiceFake*>(
        actor::ActorKeyedService::Get(profile()));
    actor_service->GetSurfaceRegistry().OnTabCreated(mock_tab_);
    actor::TaskId task_id = actor_service->CreateTaskForTesting();
    actor_service->GetTask(task_id)->AddTab(
        mock_tab_.GetHandle(), /*stop_task_on_detach=*/true, base::DoNothing());
    return task_id;
  }

  std::unique_ptr<content::WebContents> CreateWebContentsWithClient() {
    std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
    autofill::ChromeAutofillClient::CreateForWebContents(contents.get());
    ChromePasswordManagerClient::CreateForWebContents(contents.get());
    return contents;
  }

 private:
  tabs::MockTabInterface mock_tab_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  raw_ptr<MockCriticalActionService> mock_service_ = nullptr;
  std::unique_ptr<password_manager::StubPasswordManagerClient> stub_client_;
  std::unique_ptr<password_manager::PasswordManagerCriticalActionLogger>
      logger_;
  GURL test_url_ = GURL("https://example.com");
  int64_t nav_id_ = 0;
};

TEST_F(PasswordManagerCriticalActionLoggerTest,
       LogsCriticalActionWhenActorTaskIsActive) {
  actor::TaskId task_id = StartActorTask();

  critical_actions::CriticalActionEntry recorded_entry;
  int64_t recorded_nav_id = 0;

  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId)
      .WillOnce(testing::DoAll(SaveArg<0>(&recorded_entry),
                               SaveArg<1>(&recorded_nav_id)));

  logger()->MaybeLogCriticalAction(
      critical_actions::ActionType::kGooglePasswordManager,
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          web_contents()->GetPrimaryMainFrame()));

  EXPECT_EQ(recorded_nav_id, nav_id());
  EXPECT_EQ(recorded_entry.action_type,
            critical_actions::ActionType::kGooglePasswordManager);
  EXPECT_EQ(recorded_entry.action_source,
            critical_actions::ActionSource::kPasswordManager);
  EXPECT_EQ(recorded_entry.actor_task_id,
            base::NumberToString(task_id.value()));
}

TEST_F(PasswordManagerCriticalActionLoggerTest,
       LogsFederatedLoginCriticalActionWhenActorTaskIsActive) {
  actor::TaskId task_id = StartActorTask();

  critical_actions::CriticalActionEntry recorded_entry;
  int64_t recorded_nav_id = 0;

  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId)
      .WillOnce(testing::DoAll(SaveArg<0>(&recorded_entry),
                               SaveArg<1>(&recorded_nav_id)));

  logger()->MaybeLogCriticalAction(
      critical_actions::ActionType::kFederatedLogin);

  EXPECT_EQ(recorded_nav_id, nav_id());
  EXPECT_EQ(recorded_entry.action_type,
            critical_actions::ActionType::kFederatedLogin);
  EXPECT_EQ(recorded_entry.action_source,
            critical_actions::ActionSource::kPasswordManager);
  EXPECT_EQ(recorded_entry.actor_task_id,
            base::NumberToString(task_id.value()));
}

TEST_F(PasswordManagerCriticalActionLoggerTest,
       DoesNotLogCriticalActionWhenNoActorTaskIsActive) {
  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId).Times(0);

  logger()->MaybeLogCriticalAction(
      critical_actions::ActionType::kGooglePasswordManager,
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          web_contents()->GetPrimaryMainFrame()));
}

TEST_F(PasswordManagerCriticalActionLoggerTest,
       DoesNotLogCriticalActionIfFeatureDisabled) {
  StartActorTask();

  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(
      critical_actions::features::kCriticalActionHistory);

  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId).Times(0);

  logger()->MaybeLogCriticalAction(
      critical_actions::ActionType::kGooglePasswordManager,
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          web_contents()->GetPrimaryMainFrame()));
}

TEST_F(PasswordManagerCriticalActionLoggerTest,
       NotifiesCriticalActionServiceOnUncommittedNavigation) {
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/other"), web_contents());
  simulator->Start();
  int64_t uncommitted_nav_id =
      simulator->GetNavigationHandle()->GetNavigationId();

  EXPECT_CALL(*mock_service(), OnNavigationDiscarded(uncommitted_nav_id));

  simulator->Fail(net::ERR_ABORTED);

  // Verify that the call happened and clear expectations so that
  // TearDown's DeleteContents() doesn't trigger noise.
  testing::Mock::VerifyAndClearExpectations(mock_service());
}

TEST_F(PasswordManagerCriticalActionLoggerTest,
       OnFedCmFederatedLoginLogsCriticalAction) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateWebContentsWithClient();
  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(web_contents.get());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents.get(),
                                                       mock_tab());

  int64_t expected_nav_id = CommitNavigation(web_contents.get());

  actor::TaskId task_id = StartActorTask();

  // Failed FedCM login should not log a critical action.
  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId).Times(0);
  static_cast<content::WebContentsObserver*>(client)->OnFedCmFederatedLogin(
      false);

  // Successful FedCM login should log a kFederatedLogin critical action.
  critical_actions::CriticalActionEntry recorded_entry;
  int64_t recorded_nav_id = 0;
  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId)
      .WillOnce(testing::DoAll(SaveArg<0>(&recorded_entry),
                               SaveArg<1>(&recorded_nav_id)));
  static_cast<content::WebContentsObserver*>(client)->OnFedCmFederatedLogin(
      true);

  EXPECT_EQ(recorded_nav_id, expected_nav_id);
  EXPECT_EQ(recorded_entry.action_type,
            critical_actions::ActionType::kFederatedLogin);
  EXPECT_EQ(recorded_entry.action_source,
            critical_actions::ActionSource::kPasswordManager);
  EXPECT_EQ(recorded_entry.actor_task_id,
            base::NumberToString(task_id.value()));
}

// Verifies that when federated login occurs in a Relying Party (RP) tab with an
// active Actor task, the critical action is logged on the RP tab itself even if
// it has an opener.
TEST_F(PasswordManagerCriticalActionLoggerTest,
       OnFedCmFederatedLoginWhenTabHasOpenerLogsCriticalAction) {
  std::unique_ptr<content::WebContents> opener_web_contents =
      CreateWebContentsWithClient();
  int64_t opener_nav_id = CommitNavigation(opener_web_contents.get());

  std::unique_ptr<content::WebContents> rp_web_contents =
      CreateWebContentsWithClient();
  ChromePasswordManagerClient* rp_client =
      ChromePasswordManagerClient::FromWebContents(rp_web_contents.get());
  tabs::TabLookupFromWebContents::CreateForWebContents(rp_web_contents.get(),
                                                       mock_tab());
  content::WebContentsTester::For(rp_web_contents.get())
      ->SetOpener(opener_web_contents.get());

  int64_t expected_nav_id = CommitNavigation(rp_web_contents.get());
  ASSERT_NE(expected_nav_id, opener_nav_id);

  actor::TaskId task_id = StartActorTask();

  critical_actions::CriticalActionEntry recorded_entry;
  int64_t recorded_nav_id = 0;
  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId)
      .WillOnce(testing::DoAll(SaveArg<0>(&recorded_entry),
                               SaveArg<1>(&recorded_nav_id)));

  static_cast<content::WebContentsObserver*>(rp_client)->OnFedCmFederatedLogin(
      true);

  EXPECT_EQ(recorded_nav_id, expected_nav_id);
  EXPECT_EQ(recorded_entry.action_type,
            critical_actions::ActionType::kFederatedLogin);
  EXPECT_EQ(recorded_entry.action_source,
            critical_actions::ActionSource::kPasswordManager);
  EXPECT_EQ(recorded_entry.actor_task_id,
            base::NumberToString(task_id.value()));
}

// Verifies that when federated login occurs in a popup window (which has no
// active Actor task), critical action logging is delegated to its opener tab.
TEST_F(PasswordManagerCriticalActionLoggerTest,
       OnNonPasswordLoginDetectedDelegationLogsCriticalAction) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateWebContentsWithClient();
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents.get(),
                                                       mock_tab());

  int64_t expected_nav_id = CommitNavigation(web_contents.get());

  actor::TaskId task_id = StartActorTask();

  // Create a popup WebContents with web_contents as its opener.
  std::unique_ptr<content::WebContents> popup_web_contents =
      CreateWebContentsWithClient();
  ChromePasswordManagerClient* popup_client =
      ChromePasswordManagerClient::FromWebContents(popup_web_contents.get());
  content::WebContentsTester::For(popup_web_contents.get())
      ->SetOpener(web_contents.get());

  critical_actions::CriticalActionEntry recorded_entry;
  int64_t recorded_nav_id = 0;
  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId)
      .WillOnce(testing::DoAll(SaveArg<0>(&recorded_entry),
                               SaveArg<1>(&recorded_nav_id)));

  popup_client->OnNonFedCmFederatedLogin();

  EXPECT_EQ(recorded_nav_id, expected_nav_id);
  EXPECT_EQ(recorded_entry.action_type,
            critical_actions::ActionType::kFederatedLogin);
  EXPECT_EQ(recorded_entry.action_source,
            critical_actions::ActionSource::kPasswordManager);
  EXPECT_EQ(recorded_entry.actor_task_id,
            base::NumberToString(task_id.value()));
}
