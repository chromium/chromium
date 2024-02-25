// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"

#include <memory>

#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#else
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#endif

namespace policy {

namespace {

constexpr char kAffiliationId1[] = "affiliation-id-1";
constexpr char kAffiliationId2[] = "affiliation-id-2";
constexpr char kDMToken[] = "dm-token";

}  // namespace

class UserPolicySigninServiceUtilTest : public ::testing::Test {
 public:
  UserPolicySigninServiceUtilTest() = default;
  ~UserPolicySigninServiceUtilTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    init_params->device_properties = crosapi::mojom::DeviceProperties::New();
    init_params->device_properties->device_affiliation_ids = {kAffiliationId1};
    init_params->device_properties->device_dm_token = kDMToken;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#else
    fake_storage_ = std::make_unique<FakeBrowserDMTokenStorage>();
    fake_storage_->SetClientId("client-id");
    fake_storage_->SetDMToken(kDMToken);

    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->add_device_affiliation_ids(kAffiliationId1);

    auto store = std::make_unique<MachineLevelUserCloudPolicyStore>(
        DMToken::CreateValidToken(kDMToken), std::string(), base::FilePath(),
        base::FilePath(), base::FilePath(), base::FilePath(),
        scoped_refptr<base::SequencedTaskRunner>());
    store->set_policy_data_for_testing(std::move(policy_data));

    manager_ = std::make_unique<MachineLevelUserCloudPolicyManager>(
        std::move(store), /*external_data_manager=*/nullptr,
        /*policy_dir=*/base::FilePath(),
        scoped_refptr<base::SequencedTaskRunner>(),
        network::TestNetworkConnectionTracker::CreateGetter());

    auto client = std::make_unique<CloudPolicyClient>(
        /*service=*/nullptr, /*url_laoder_factory=*/nullptr,
        CloudPolicyClient::DeviceDMTokenCallback());
    client->SetupRegistration(kDMToken, "client-id", {kAffiliationId1});
    manager_->core()->ConnectForTesting(/*service=*/nullptr, std::move(client));
    g_browser_process->browser_policy_connector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(manager_.get());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  void TearDown() override {
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    g_browser_process->browser_policy_connector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
  }

 private:
  base::test::TaskEnvironment task_env_;

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<FakeBrowserDMTokenStorage> fake_storage_;
  std::unique_ptr<MachineLevelUserCloudPolicyManager> manager_;
#endif
};

TEST_F(UserPolicySigninServiceUtilTest, Affiliated) {
  EXPECT_EQ(kDMToken, GetDeviceDMTokenIfAffiliated({kAffiliationId1}));
}

TEST_F(UserPolicySigninServiceUtilTest, NotAffiliated) {
  EXPECT_EQ(std::string(), GetDeviceDMTokenIfAffiliated({kAffiliationId2}));
}

}  // namespace policy
