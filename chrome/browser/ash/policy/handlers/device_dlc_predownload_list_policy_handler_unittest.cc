// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_dlc_predownload_list_policy_handler.h"

#include <memory>
#include <string_view>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

using google::protobuf::RepeatedPtrField;

namespace policy {

class DeviceDlcPredownloadListPolicyHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    ash::DlcserviceClient::InitializeFake();
    dlc_predownloader_ = DeviceDlcPredownloadListPolicyHandler::Create();
  }

  void TearDown() override { ash::DlcserviceClient::Shutdown(); }

  void SetDeviceDlcPredownloadPolicy(base::Value value) {
    testing_cros_settings_.device_settings()->Set(
        ash::kDeviceDlcPredownloadList, value);
  }

  void SetDlcInstallError(const std::string& err) {
    fake_dlcservice_client()->set_install_error(err);
  }

 private:
  ash::FakeDlcserviceClient* fake_dlcservice_client() {
    return static_cast<ash::FakeDlcserviceClient*>(
        ash::DlcserviceClient::Get());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  ash::ScopedTestingCrosSettings testing_cros_settings_;
  std::unique_ptr<DeviceDlcPredownloadListPolicyHandler> dlc_predownloader_;
};

void RecordGetExistingDlcsResult(std::string& out_err,
                                 dlcservice::DlcsWithContent& out_dlcs,
                                 std::string_view err,
                                 const dlcservice::DlcsWithContent& dlcs) {
  out_dlcs = dlcs;
  out_err = err;
}

TEST_F(DeviceDlcPredownloadListPolicyHandlerTest, InstallSuccess) {
  SetDeviceDlcPredownloadPolicy(
      base::Value(base::Value::List().Append("scanner_drivers")));

  std::string err;
  dlcservice::DlcsWithContent dlcs;
  ash::DlcserviceClient::Get()->GetExistingDlcs(base::BindOnce(
      &RecordGetExistingDlcsResult, std::ref(err), std::ref(dlcs)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(err, dlcservice::kErrorNone);
  EXPECT_EQ(dlcs.dlc_infos_size(), 1);
  EXPECT_EQ(dlcs.dlc_infos().at(0).id(), "scanner_drivers");
}

TEST(DecodeDeviceDlcPredownloadListPolicy, EmptyList) {
  std::string warning;

  base::Value::List decoded_policies = DeviceDlcPredownloadListPolicyHandler::
      DecodeDeviceDlcPredownloadListPolicy({}, warning);

  EXPECT_TRUE(warning.empty());
  EXPECT_EQ(decoded_policies, base::Value::List());
}

TEST(DecodeDeviceDlcPredownloadListPolicy, OnlyValidValues) {
  std::string warning;
  RepeatedPtrField<std::string> policy;
  policy.Add("scanner_drivers");

  base::Value::List decoded_policies = DeviceDlcPredownloadListPolicyHandler::
      DecodeDeviceDlcPredownloadListPolicy(policy, warning);

  EXPECT_TRUE(warning.empty());
  EXPECT_EQ(decoded_policies, base::Value::List().Append("sane-backends-pfu"));
}

TEST(DecodeDeviceDlcPredownloadListPolicy, DuplicateValidValues) {
  std::string warning;
  RepeatedPtrField<std::string> policy;
  policy.Add("scanner_drivers");
  policy.Add("scanner_drivers");

  base::Value::List decoded_policies = DeviceDlcPredownloadListPolicyHandler::
      DecodeDeviceDlcPredownloadListPolicy(policy, warning);

  EXPECT_TRUE(warning.empty());
  EXPECT_EQ(decoded_policies, base::Value::List().Append("sane-backends-pfu"));
}

TEST(DecodeDeviceDlcPredownloadListPolicy, InvalidAndValidValues) {
  std::string warning;
  RepeatedPtrField<std::string> policy;
  policy.Add("scanner_drivers");
  policy.Add("invalid_value");

  base::Value::List decoded_policies = DeviceDlcPredownloadListPolicyHandler::
      DecodeDeviceDlcPredownloadListPolicy(policy, warning);

  EXPECT_FALSE(warning.empty());
  EXPECT_EQ(decoded_policies, base::Value::List().Append("sane-backends-pfu"));
}

}  // namespace policy
