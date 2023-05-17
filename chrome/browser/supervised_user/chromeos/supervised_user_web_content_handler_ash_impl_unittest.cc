// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/supervised_user_web_content_handler_impl.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/fake_ash_parent_access_dialog_provider.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/parent_access_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/chromeos/mock_large_icon_service.h"
#include "chrome/browser/supervised_user/chromeos/supervised_user_favicon_request_handler.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// TODO(b/273692421): Extend unit test scope of all the methods in
// SupervisedUserWebContentHandlerImpl.

namespace {
const char kProfileName[] = "profile_name";
const std::u16string kSupervisedUserName = u"Supervised User Name";
}  // namespace

// This class contains tests for the public API method `RequestLocalApproval`
// which is not yet supported in Lacros. The test cases are not added in
// `SupervisedUserWebContentHandlerImplTest` as mocking the parts of the
// implementation require ash-specific dependencies.
class SupervisedUserWebContentHandlerAshImplTest : public ::testing::Test {
 public:
  SupervisedUserWebContentHandlerAshImplTest() = default;
  SupervisedUserWebContentHandlerAshImplTest(
      const SupervisedUserWebContentHandlerAshImplTest&) = delete;
  SupervisedUserWebContentHandlerAshImplTest& operator=(
      const SupervisedUserWebContentHandlerAshImplTest&) = delete;

  ~SupervisedUserWebContentHandlerAshImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Create the profile manager and an active profile.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(
        kProfileName, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        kSupervisedUserName, 0, TestingProfile::TestingFactories(),
        /*is_supervised_profile=*/true);

    // The idle service has dependencies we can't instantiate in unit tests.
    crosapi::IdleServiceAsh::DisableForTesting();
    // The crosapi manager reads the global login state.
    ash::LoginState::Initialize();
    crosapi_manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();

    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->parent_access_ash()
        ->BindReceiver(parent_access_remote_.BindNewPipeAndPassReceiver());
    dialog_provider_ = static_cast<crosapi::FakeAshParentAccessDialogProvider*>(
        crosapi::CrosapiManager::Get()
            ->crosapi_ash()
            ->parent_access_ash()
            ->SetDialogProviderForTest(
                std::make_unique<
                    crosapi::FakeAshParentAccessDialogProvider>()));
  }

  void TearDown() override {
    crosapi_manager_.reset();
    dialog_provider_ = nullptr;
    ash::LoginState::Shutdown();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(kProfileName);
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  MockLargeIconService& large_icon_service() { return large_icon_service_; }
  TestingProfile* GetProfilePtr() { return profile_.get(); }
  crosapi::FakeAshParentAccessDialogProvider* GetDialogProvider() {
    return dialog_provider_.get();
  }

 private:
  MockLargeIconService large_icon_service_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  raw_ptr<crosapi::FakeAshParentAccessDialogProvider, ExperimentalAsh>
      dialog_provider_;
  mojo::Remote<crosapi::mojom::ParentAccess> parent_access_remote_;
};

TEST_F(SupervisedUserWebContentHandlerAshImplTest,
       LocalWebApprovalApprovedChromeOSTest) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.example.com");

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler(
      web_contents.get(), url, large_icon_service(),
      /*frame_id=*/0, /*interstitial_navigation_id=*/0);
  base::MockCallback<
      SupervisedUserWebContentHandlerImpl::ApprovalRequestInitiatedCallback>
      callback;
  EXPECT_CALL(callback, Run(true));

  web_content_handler.RequestLocalApproval(url, kSupervisedUserName,
                                           callback.Get());
  std::unique_ptr<ash::ParentAccessDialog::Result> dialog_result =
      std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kApproved;
  dialog_result->parent_access_token = "ABC123";
  dialog_result->parent_access_token_expire_timestamp =
      base::Time::FromDoubleT(123456UL);

  // Forward clock by the fake approval duration and trigger approval response.
  const base::TimeDelta approval_duration = base::Seconds(35);
  task_environment().FastForwardBy(approval_duration);
  GetDialogProvider()->TriggerCallbackWithResult(std::move(dialog_result));

  histogram_tester.ExpectUniqueSample(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kApproved, 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      1);
  histogram_tester.ExpectTimeBucketCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      approval_duration, 1);
}

TEST_F(SupervisedUserWebContentHandlerAshImplTest,
       LocalWebApprovalDeclinedChromeOSTest) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.example.com");

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler(
      web_contents.get(), url, large_icon_service(),
      /*frame_id=*/0, /*interstitial_navigation_id=*/0);
  base::MockCallback<
      SupervisedUserWebContentHandlerImpl::ApprovalRequestInitiatedCallback>
      callback;
  EXPECT_CALL(callback, Run(true));

  web_content_handler.RequestLocalApproval(url, kSupervisedUserName,
                                           callback.Get());

  std::unique_ptr<ash::ParentAccessDialog::Result> dialog_result =
      std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kDeclined;

  // Forward clock by the fake approval duration and trigger rejection response.
  const base::TimeDelta approval_duration = base::Seconds(35);
  task_environment().FastForwardBy(approval_duration);
  GetDialogProvider()->TriggerCallbackWithResult(std::move(dialog_result));

  histogram_tester.ExpectUniqueSample(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kDeclined, 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      1);
  histogram_tester.ExpectTimeBucketCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      approval_duration, 1);
}

TEST_F(SupervisedUserWebContentHandlerAshImplTest,
       LocalWebApprovalCanceledChromeOSTest) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.example.com");

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler(
      web_contents.get(), url, large_icon_service(),
      /*frame_id=*/0, /*interstitial_navigation_id=*/0);
  base::MockCallback<
      SupervisedUserWebContentHandlerImpl::ApprovalRequestInitiatedCallback>
      callback;
  EXPECT_CALL(callback, Run(true));

  web_content_handler.RequestLocalApproval(url, kSupervisedUserName,
                                           callback.Get());

  std::unique_ptr<ash::ParentAccessDialog::Result> dialog_result =
      std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kCanceled;
  GetDialogProvider()->TriggerCallbackWithResult(std::move(dialog_result));

  // Check that the approval duration was NOT recorded for canceled request.
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      0);
  histogram_tester.ExpectUniqueSample(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kCanceled, 1);
}

TEST_F(SupervisedUserWebContentHandlerAshImplTest,
       LocalWebApprovalErrorChromeOSTest) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.example.com");

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler(
      web_contents.get(), url, large_icon_service(),
      /*frame_id=*/0, /*interstitial_navigation_id=*/0);
  base::MockCallback<
      SupervisedUserWebContentHandlerImpl::ApprovalRequestInitiatedCallback>
      callback;
  EXPECT_CALL(callback, Run(true));

  web_content_handler.RequestLocalApproval(url, kSupervisedUserName,
                                           callback.Get());

  std::unique_ptr<ash::ParentAccessDialog::Result> dialog_result =
      std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kError;
  GetDialogProvider()->TriggerCallbackWithResult(std::move(dialog_result));

  // Check that the approval duration was NOT recorded on error.
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      0);
  histogram_tester.ExpectUniqueSample(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kError, 1);
}
