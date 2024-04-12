// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/feedback/system_logs/log_sources/family_info_log_source.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

class FamilyInfoLogSourceTest : public ::testing::Test {
 protected:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &test_url_loader_factory_);
  }

  void SimulateResponseForPendingRequest() {
    kidsmanagement::ListMembersResponse response;
    supervised_user::SetFamilyMemberAttributesForTesting(
        response.add_members(), kidsmanagement::CHILD, "user_child");
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/families/"
        "mine/members?alt=proto",
        response.SerializeAsString());
  }

  content::BrowserTaskEnvironment task_env_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(FamilyInfoLogSourceTest, FetchMemberSignedInBeforeDeadline) {
  identity_test_env_.MakePrimaryAccountAvailable("user_child@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  FamilyInfoLogSource source(identity_test_env_.identity_manager(),
                             url_loader_factory());
  std::unique_ptr<system_logs::SystemLogsResponse> response;
  source.Fetch(base::BindLambdaForTesting(
      [&](std::unique_ptr<system_logs::SystemLogsResponse> r) {
        response = std::move(r);
        run_loop.Quit();
      }));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  SimulateResponseForPendingRequest();
  run_loop.Run();

  EXPECT_EQ("child",
            response->at(supervised_user::kFamilyMemberRoleFeedbackTag));
  histogram_tester.ExpectUniqueSample(kFamilyInfoLogSourceFetchStatusUma,
                                      FamilyInfoLogSource::FetchStatus::kOk, 1);
  histogram_tester.ExpectUniqueTimeSample(kFamilyInfoLogSourceFetchLatencyUma,
                                          base::Seconds(0), 1);
}

TEST_F(FamilyInfoLogSourceTest, FetchMemberSignedInRequestTimeout) {
  identity_test_env_.MakePrimaryAccountAvailable("user_child@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  FamilyInfoLogSource source(identity_test_env_.identity_manager(),
                             url_loader_factory());
  std::unique_ptr<system_logs::SystemLogsResponse> response;
  source.Fetch(base::BindLambdaForTesting(
      [&](std::unique_ptr<system_logs::SystemLogsResponse> r) {
        response = std::move(r);
        run_loop.Quit();
      }));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  // Move fake time past the 3s deadline.
  task_env_.FastForwardBy(base::Seconds(10));
  run_loop.Run();

  EXPECT_FALSE(response->count(supervised_user::kFamilyMemberRoleFeedbackTag));
  histogram_tester.ExpectUniqueSample(
      kFamilyInfoLogSourceFetchStatusUma,
      FamilyInfoLogSource::FetchStatus::kTimeout, 1);
  histogram_tester.ExpectUniqueTimeSample(kFamilyInfoLogSourceFetchLatencyUma,
                                          base::Seconds(3), 1);
}

TEST_F(FamilyInfoLogSourceTest,
       FetchMemberSignedInWithRequestCompleteAfterDeadline) {
  identity_test_env_.MakePrimaryAccountAvailable("user_child@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::RunLoop run_loop;
  FamilyInfoLogSource source(identity_test_env_.identity_manager(),
                             url_loader_factory());
  std::unique_ptr<system_logs::SystemLogsResponse> response;
  source.Fetch(base::BindLambdaForTesting(
      [&](std::unique_ptr<system_logs::SystemLogsResponse> r) {
        response = std::move(r);
        run_loop.Quit();
      }));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  SimulateResponseForPendingRequest();

  // Move fake time passed the deadline.
  task_env_.FastForwardBy(base::Seconds(10));
  run_loop.Run();

  EXPECT_EQ("child",
            response->at(supervised_user::kFamilyMemberRoleFeedbackTag));
}

TEST_F(FamilyInfoLogSourceTest, FetchMemberSignedOut) {
  base::RunLoop run_loop;
  FamilyInfoLogSource source(identity_test_env_.identity_manager(),
                             url_loader_factory());
  std::unique_ptr<system_logs::SystemLogsResponse> response;
  source.Fetch(base::BindLambdaForTesting(
      [&](std::unique_ptr<system_logs::SystemLogsResponse> r) {
        response = std::move(r);
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_FALSE(response->count(supervised_user::kFamilyMemberRoleFeedbackTag));
}

}  // namespace system_logs
