// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/spotlight/spotlight_crd_manager_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/boca/spotlight/spotlight_notification_bubble_controller.h"
#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/crd_session_result_codes.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::TestFuture;
using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {
namespace {
constexpr char kSpotlightConnectionCode[] = "123";
constexpr char kUserEmail[] = "cat@gmail.com";
constexpr char kCrdResultUma[] = "Enterprise.Boca.Spotlight.Crd.Result";

class MockSharedCrdSession : public policy::SharedCrdSession {
 public:
  ~MockSharedCrdSession() override = default;

  MOCK_METHOD(void,
              StartCrdHost,
              (const SessionParameters& parameters,
               AccessCodeCallback success_callback,
               ErrorCallback error_callback,
               SessionFinishedCallback session_finished_callback),
              (override));
  MOCK_METHOD(void, TerminateSession, (), (override));
};

class SpotlightCrdManagerImplTest : public InProcessBrowserTest {
 protected:
  SpotlightCrdManagerImplTest() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kBoca, ash::features::kBocaConsumer,
         ash::features::kBocaSpotlight,
         ash::features::kOnDeviceSpeechRecognition},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    auto crd_session = std::make_unique<NiceMock<MockSharedCrdSession>>();
    crd_session_ = crd_session.get();
    auto notification_bubble_controller =
        std::make_unique<ash::SpotlightNotificationBubbleController>();
    notification_bubble_controller_ = notification_bubble_controller.get();
    manager_ = std::make_unique<SpotlightCrdManagerImpl>(
        std::move(crd_session), std::move(notification_bubble_controller));
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Profile* profile() { return browser()->profile(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<SpotlightCrdManagerImpl> manager_;
  raw_ptr<NiceMock<MockSharedCrdSession>> crd_session_;
  raw_ptr<ash::SpotlightNotificationBubbleController>
      notification_bubble_controller_;
};

IN_PROC_BROWSER_TEST_F(SpotlightCrdManagerImplTest,
                       InitiateSpotlightSessionShouldStartCrdHost) {
  EXPECT_CALL(*crd_session_, StartCrdHost)
      .WillOnce(WithArg<1>(
          Invoke([&](auto callback) { std::move(callback).Run("123"); })));
  TestFuture<const std::string&> success_future;

  manager_->InitiateSpotlightSession(success_future.GetCallback(), kUserEmail);
  ::testing::Mock::VerifyAndClearExpectations(crd_session_);

  EXPECT_EQ(kSpotlightConnectionCode, success_future.Get());
}

IN_PROC_BROWSER_TEST_F(
    SpotlightCrdManagerImplTest,
    InitiateSpotlightSessionWithCrdFailureShouldRunErrorCallback) {
  base::HistogramTester histograms;

  TestFuture<void> error_callback_future;
  EXPECT_CALL(*crd_session_, StartCrdHost)
      .WillOnce(WithArg<2>(Invoke([&](auto callback) {
        base::UmaHistogramEnumeration(
            kCrdResultUma,
            policy::ExtendedStartCrdSessionResultCode::kFailureCrdHostError);
        error_callback_future.SetValue();
      })));

  manager_->InitiateSpotlightSession(
      base::BindOnce([](const std::string& result) {
        GTEST_FAIL() << "Unexpected call to success callback";
      }),
      kUserEmail);
  ::testing::Mock::VerifyAndClearExpectations(crd_session_);

  EXPECT_TRUE(error_callback_future.Wait());
  EXPECT_EQ(
      histograms.GetBucketCount(
          kCrdResultUma,
          policy::ExtendedStartCrdSessionResultCode::kFailureCrdHostError),
      1);
}

IN_PROC_BROWSER_TEST_F(SpotlightCrdManagerImplTest,
                       OnSessionEndedShouldTerminateSession) {
  EXPECT_CALL(*crd_session_, StartCrdHost).Times(1);
  EXPECT_CALL(*crd_session_, TerminateSession()).Times(1);
  TestFuture<const std::string&> success_future;

  manager_->InitiateSpotlightSession(success_future.GetCallback(), kUserEmail);

  manager_->OnSessionEnded();
  ::testing::Mock::VerifyAndClearExpectations(crd_session_);
}

IN_PROC_BROWSER_TEST_F(SpotlightCrdManagerImplTest,
                       ShowPersistentNotificationShowsWidget) {
  manager_->ShowPersistentNotification("Teacher");

  EXPECT_TRUE(notification_bubble_controller_->IsNotificationBubbleVisible());
}

IN_PROC_BROWSER_TEST_F(SpotlightCrdManagerImplTest,
                       HidePersistentNotificationHidesWidget) {
  manager_->ShowPersistentNotification("Teacher");

  manager_->HidePersistentNotification();
  EXPECT_FALSE(notification_bubble_controller_->IsNotificationBubbleVisible());
}

IN_PROC_BROWSER_TEST_F(SpotlightCrdManagerImplTest,
                       HidesNotificationWidgetOnSessionEnd) {
  manager_->ShowPersistentNotification("Teacher");

  manager_->OnSessionEnded();
  ::testing::Mock::VerifyAndClearExpectations(crd_session_);
  EXPECT_FALSE(notification_bubble_controller_->IsNotificationBubbleVisible());
}

IN_PROC_BROWSER_TEST_F(SpotlightCrdManagerImplTest,
                       TerminatesCrdSessionOnSessionEnd) {
  TestFuture<void> session_finished_future;
  EXPECT_CALL(*crd_session_, TerminateSession)
      .WillOnce(InvokeWithoutArgs(
          [&]() { std::move(session_finished_future.GetCallback()).Run(); }));

  TestFuture<const std::string&> success_future;
  manager_->InitiateSpotlightSession(success_future.GetCallback(), kUserEmail);

  manager_->OnSessionEnded();
  ::testing::Mock::VerifyAndClearExpectations(crd_session_);
  EXPECT_TRUE(session_finished_future.Wait());
}

}  // namespace
}  // namespace ash::boca
