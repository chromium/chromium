// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/scoped_multi_source_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#include "chrome/browser/sync/test/integration/contact_info_helper.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/power_bookmarks/common/power.h"
#include "components/power_bookmarks/common/power_bookmark_observer.h"
#include "components/power_bookmarks/common/power_test_util.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/sync/base/features.h"
#include "components/sync/test/fake_server_http_post_provider.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::ContainerEq;

std::string PowerToString(const power_bookmarks::Power& power) {
  sync_pb::PowerBookmarkSpecifics specifics;
  power.ToPowerBookmarkSpecifics(&specifics);
  return specifics.SerializeAsString();
}

std::vector<std::string> GetPowersForURLAsString(
    GURL url,
    power_bookmarks::PowerBookmarkService* service) {
  base::RunLoop run_loop;
  std::vector<std::string> result;
  service->GetPowersForURL(
      url, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
      base::BindOnce(
          [](base::RunLoop* run_loop, std::vector<std::string>* result,
             std::vector<std::unique_ptr<power_bookmarks::Power>> powers) {
            for (auto& power : powers) {
              result->push_back(PowerToString(*power));
            }
            run_loop->Quit();
          },
          &run_loop, &result));
  run_loop.Run();
  return result;
}

// Helper class to wait until the two services match for the given `url`.
class PowerBookmarkChecker : public StatusChangeChecker,
                             public power_bookmarks::PowerBookmarkObserver {
 public:
  PowerBookmarkChecker(power_bookmarks::PowerBookmarkService* service0,
                       power_bookmarks::PowerBookmarkService* service1,
                       GURL url)
      : service0_(service0), service1_(service1), url_(url) {
    power_bookmark_service_obs_.AddObservation(service0_);
    power_bookmark_service_obs_.AddObservation(service1_);
  }

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    testing::StringMatchResultListener result_listener;
    bool matches =
        ExplainMatchResult(testing::UnorderedElementsAreArray(service0_data_),
                           service1_data_, &result_listener);
    *os << result_listener.str();
    return matches;
  }

  // power_bookmarks::PowerBookmarkObserver implementation.
  void OnPowersChanged() override {
    service0_data_ = GetPowersForURLAsString(url_, service0_);
    service1_data_ = GetPowersForURLAsString(url_, service1_);

    CheckExitCondition();
  }

 private:
  const raw_ptr<power_bookmarks::PowerBookmarkService> service0_;
  const raw_ptr<power_bookmarks::PowerBookmarkService> service1_;
  const GURL url_;

  base::ScopedMultiSourceObservation<power_bookmarks::PowerBookmarkService,
                                     power_bookmarks::PowerBookmarkObserver>
      power_bookmark_service_obs_{this};
  std::vector<std::string> service0_data_;
  std::vector<std::string> service1_data_;
};

class TwoClientPowerBookmarksSyncTest : public SyncTest {
 public:
  const GURL kGoogleURL = GURL("https://google.com");
  TwoClientPowerBookmarksSyncTest() : SyncTest(TWO_CLIENT) {
    features_.InitWithFeatures(
        /*enabled_features=*/{power_bookmarks::kPowerBookmarkBackend},
        /*disabled_features=*/{});
  }

  void SetupServices() {
    service0_ = GetSyncService(0);
    service1_ = GetSyncService(1);
  }

 protected:
  raw_ptr<power_bookmarks::PowerBookmarkService, AcrossTasksDanglingUntriaged>
      service0_;
  raw_ptr<power_bookmarks::PowerBookmarkService, AcrossTasksDanglingUntriaged>
      service1_;

 private:
  power_bookmarks::PowerBookmarkService* GetSyncService(int index) {
    return PowerBookmarkServiceFactory::GetForBrowserContext(GetProfile(index));
  }

  base::test::ScopedFeatureList features_;
};

bool CreatePower(std::unique_ptr<power_bookmarks::Power> power,
                 power_bookmarks::PowerBookmarkService* service) {
  base::RunLoop run_loop;
  bool result = false;
  service->CreatePower(std::move(power),
                       base::BindLambdaForTesting([&](bool success) {
                         result = success;
                         run_loop.Quit();
                       }));
  run_loop.Run();
  return result;
}

bool UpdatePower(std::unique_ptr<power_bookmarks::Power> power,
                 power_bookmarks::PowerBookmarkService* service) {
  base::RunLoop run_loop;
  bool result = false;
  service->UpdatePower(std::move(power),
                       base::BindLambdaForTesting([&](bool success) {
                         result = success;
                         run_loop.Quit();
                       }));
  run_loop.Run();
  return result;
}

bool DeletePower(base::Uuid guid,
                 power_bookmarks::PowerBookmarkService* service) {
  base::RunLoop run_loop;
  bool result = false;
  service->DeletePower(guid, base::BindLambdaForTesting([&](bool success) {
                         result = success;
                         run_loop.Quit();
                       }));
  run_loop.Run();
  return result;
}

bool DeletePowersForURL(GURL url,
                        power_bookmarks::PowerBookmarkService* service) {
  base::RunLoop run_loop;
  bool result = false;
  service->DeletePowersForURL(url,
                              sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
                              base::BindLambdaForTesting([&](bool success) {
                                result = success;
                                run_loop.Quit();
                              }));
  run_loop.Run();
  return result;
}
void VerifyPowersForURL(GURL url,
                        power_bookmarks::PowerBookmarkService* service0,
                        power_bookmarks::PowerBookmarkService* service1) {
  EXPECT_THAT(GetPowersForURLAsString(url, service0),
              ContainerEq(GetPowersForURLAsString(url, service1)));
}

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class TwoClientPowerBookmarksSyncTestNoTestingConfig
    : public TwoClientPowerBookmarksSyncTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TwoClientPowerBookmarksSyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

IN_PROC_BROWSER_TEST_F(TwoClientPowerBookmarksSyncTestNoTestingConfig,
                       AddOnePower) {
  ASSERT_TRUE(SetupSync());
  SetupServices();

  // service0 adds a new power.
  auto power1 = power_bookmarks::MakePower(
      kGoogleURL, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_TRUE(CreatePower(power1->Clone(), service0_));
  EXPECT_TRUE(PowerBookmarkChecker(service0_, service1_, kGoogleURL).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPowerBookmarksSyncTest,
                       // TODO(crbug.com/40901832): Re-enable this test.
                       DISABLED_UpdateOnePower) {
  ASSERT_TRUE(SetupSync());
  SetupServices();

  // service0 adds a new power.
  auto power1 = power_bookmarks::MakePower(
      kGoogleURL, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  base::Time now = base::Time::Now();
  power1->set_time_modified(now);
  EXPECT_TRUE(CreatePower(power1->Clone(), service0_));
  EXPECT_TRUE(PowerBookmarkChecker(service0_, service1_, kGoogleURL).Wait());
  VerifyPowersForURL(kGoogleURL, service0_, service1_);

  // service1 updates an existing power.
  power1->set_time_modified(now + base::Seconds(1));
  EXPECT_TRUE(UpdatePower(std::move(power1), service1_));
  EXPECT_TRUE(PowerBookmarkChecker(service0_, service1_, kGoogleURL).Wait());
  VerifyPowersForURL(kGoogleURL, service0_, service1_);
}

IN_PROC_BROWSER_TEST_F(TwoClientPowerBookmarksSyncTest, DeleteOnePower) {
  ASSERT_TRUE(SetupSync());
  SetupServices();

  // service0 adds a new power.
  auto power1 = power_bookmarks::MakePower(
      kGoogleURL, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  auto guid = power1->guid();
  EXPECT_TRUE(CreatePower(power1->Clone(), service0_));
  EXPECT_TRUE(PowerBookmarkChecker(service0_, service1_, kGoogleURL).Wait());

  // service0 deletes a power.
  EXPECT_TRUE(DeletePower(guid, service0_));
  EXPECT_TRUE(PowerBookmarkChecker(service0_, service1_, kGoogleURL).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPowerBookmarksSyncTestNoTestingConfig,
                       AddMultiplePowers) {
  ASSERT_TRUE(SetupSync());
  SetupServices();

  // service0 adds a power.
  base::RunLoop run_loop1;
  auto power1 = power_bookmarks::MakePower(
      kGoogleURL, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_TRUE(CreatePower(std::move(power1), service0_));
  EXPECT_TRUE(PowerBookmarkChecker(service0_, service1_, kGoogleURL).Wait());

  // service0 adds another power.
  base::RunLoop run_loop3;
  auto power2 = power_bookmarks::MakePower(
      kGoogleURL, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_TRUE(CreatePower(std::move(power2), service0_));
  EXPECT_TRUE(PowerBookmarkChecker(service0_, service1_, kGoogleURL).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPowerBookmarksSyncTest, DeletePowersForURL) {
  ASSERT_TRUE(SetupSync());
  SetupServices();

  // service0 adds a power.
  base::RunLoop run_loop1;
  auto power1 = power_bookmarks::MakePower(
      kGoogleURL, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_TRUE(CreatePower(std::move(power1), service0_));
  EXPECT_TRUE(PowerBookmarkChecker(service0_, service1_, kGoogleURL).Wait());

  // service0 adds another power.
  base::RunLoop run_loop3;
  auto power2 = power_bookmarks::MakePower(
      kGoogleURL, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  EXPECT_TRUE(CreatePower(std::move(power2), service0_));
  EXPECT_TRUE(PowerBookmarkChecker(service0_, service1_, kGoogleURL).Wait());

  // service0 deletes powers for URL.
  EXPECT_TRUE(DeletePowersForURL(kGoogleURL, service0_));
  EXPECT_TRUE(PowerBookmarkChecker(service0_, service1_, kGoogleURL).Wait());
}

}  // namespace
