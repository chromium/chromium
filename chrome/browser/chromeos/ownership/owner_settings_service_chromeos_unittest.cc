// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

void OnPrefChanged(const std::string& /* setting */) {}

class PrefsChecker : public ownership::OwnerSettingsService::Observer {
 public:
  PrefsChecker(OwnerSettingsServiceChromeOS* service,
               DeviceSettingsProvider* provider)
      : service_(service), provider_(provider) {
    CHECK(service_);
    CHECK(provider_);
    service_->AddObserver(this);
  }

  ~PrefsChecker() override { service_->RemoveObserver(this); }

  // OwnerSettingsService::Observer implementation:
  void OnSignedPolicyStored(bool success) override {
    if (service_->HasPendingChanges())
      return;

    while (!set_requests_.empty()) {
      SetRequest request = std::move(set_requests_.front());
      set_requests_.pop();
      const base::Value* value = provider_->Get(request.first);
      ASSERT_EQ(request.second, *value);
    }
    loop_.Quit();
  }

  bool Set(const std::string& setting, const base::Value& value) {
    if (!service_->Set(setting, value))
      return false;
    set_requests_.push(SetRequest(setting, value.Clone()));
    return true;
  }

  void Wait() { loop_.Run(); }

 private:
  OwnerSettingsServiceChromeOS* service_;
  DeviceSettingsProvider* provider_;
  base::RunLoop loop_;

  using SetRequest = std::pair<std::string, base::Value>;
  base::queue<SetRequest> set_requests_;

  DISALLOW_COPY_AND_ASSIGN(PrefsChecker);
};

bool FindInListValue(const std::string& needle, const base::Value* haystack) {
  const base::ListValue* list;
  if (!haystack->GetAsList(&list))
    return false;
  return list->end() != list->Find(base::Value(needle));
}

}  // namespace

class OwnerSettingsServiceChromeOSTest : public DeviceSettingsTestBase {
 public:
  OwnerSettingsServiceChromeOSTest()
      : service_(nullptr),
        local_state_(TestingBrowserProcess::GetGlobal()),
        user_data_dir_override_(chrome::DIR_USER_DATA),
        management_settings_set_(false) {}

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    provider_.reset(new DeviceSettingsProvider(
        base::Bind(&OnPrefChanged), device_settings_service_.get(),
        TestingBrowserProcess::GetGlobal()->local_state()));
    owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());
    InitOwner(
        AccountId::FromUserEmail(device_policy_->policy_data().username()),
        true);
    FlushDeviceSettings();

    service_ = OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
        profile_.get());
    ASSERT_TRUE(service_);
    ASSERT_TRUE(service_->IsOwner());

    device_policy_->policy_data().set_management_mode(
        em::PolicyData::LOCAL_OWNER);
    device_policy_->Build();
    session_manager_client_.set_device_policy(device_policy_->GetBlob());
    ReloadDeviceSettings();
  }

  void TearDown() override {
    provider_.reset();
    DeviceSettingsTestBase::TearDown();
  }

  void TestSingleSet(OwnerSettingsServiceChromeOS* service,
                     const std::string& setting,
                     const base::Value& in_value) {
    PrefsChecker checker(service, provider_.get());
    checker.Set(setting, in_value);
    FlushDeviceSettings();
    checker.Wait();
  }

  void OnManagementSettingsSet(bool success) {
    management_settings_set_ = success;
  }

 protected:
  OwnerSettingsServiceChromeOS* service_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<DeviceSettingsProvider> provider_;
  base::ScopedPathOverride user_data_dir_override_;
  bool management_settings_set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceChromeOSTest);
};

TEST_F(OwnerSettingsServiceChromeOSTest, SingleSetTest) {
  TestSingleSet(service_, kReleaseChannel, base::Value("dev-channel"));
  TestSingleSet(service_, kReleaseChannel, base::Value("beta-channel"));
  TestSingleSet(service_, kReleaseChannel, base::Value("stable-channel"));
}

TEST_F(OwnerSettingsServiceChromeOSTest, MultipleSetTest) {
  base::Value allow_guest(false);
  base::Value release_channel("stable-channel");
  base::Value show_user_names(true);

  PrefsChecker checker(service_, provider_.get());

  checker.Set(kAccountsPrefAllowGuest, allow_guest);
  checker.Set(kReleaseChannel, release_channel);
  checker.Set(kAccountsPrefShowUserNamesOnSignIn, show_user_names);

  FlushDeviceSettings();
  checker.Wait();
}

TEST_F(OwnerSettingsServiceChromeOSTest, FailedSetRequest) {
  session_manager_client_.ForceStorePolicyFailure(true);
  std::string current_channel;
  ASSERT_TRUE(provider_->Get(kReleaseChannel)->GetAsString(&current_channel));
  ASSERT_NE(current_channel, "stable-channel");

  // Check that DeviceSettingsProvider's cache is updated.
  PrefsChecker checker(service_, provider_.get());
  checker.Set(kReleaseChannel, base::Value("stable-channel"));
  FlushDeviceSettings();
  checker.Wait();

  // Check that DeviceSettingsService's policy isn't updated.
  ASSERT_EQ(current_channel, device_settings_service_->device_settings()
                                 ->release_channel()
                                 .release_channel());
}

TEST_F(OwnerSettingsServiceChromeOSTest, ForceWhitelist) {
  EXPECT_FALSE(FindInListValue(device_policy_->policy_data().username(),
                               provider_->Get(kAccountsPrefUsers)));
  // Force a settings write.
  TestSingleSet(service_, kReleaseChannel, base::Value("dev-channel"));
  EXPECT_TRUE(FindInListValue(device_policy_->policy_data().username(),
                              provider_->Get(kAccountsPrefUsers)));
}

class OwnerSettingsServiceChromeOSNoOwnerTest
    : public OwnerSettingsServiceChromeOSTest {
 public:
  OwnerSettingsServiceChromeOSNoOwnerTest() {}
  ~OwnerSettingsServiceChromeOSNoOwnerTest() override {}

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    provider_.reset(new DeviceSettingsProvider(
        base::Bind(&OnPrefChanged), device_settings_service_.get(),
        TestingBrowserProcess::GetGlobal()->local_state()));
    FlushDeviceSettings();
    service_ = OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
        profile_.get());
    ASSERT_TRUE(service_);
    ASSERT_FALSE(service_->IsOwner());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceChromeOSNoOwnerTest);
};

TEST_F(OwnerSettingsServiceChromeOSNoOwnerTest, SingleSetTest) {
  ASSERT_FALSE(service_->SetBoolean(kAccountsPrefAllowGuest, false));
}

TEST_F(OwnerSettingsServiceChromeOSNoOwnerTest, TakeOwnershipForceWhitelist) {
  EXPECT_FALSE(FindInListValue(device_policy_->policy_data().username(),
                               provider_->Get(kAccountsPrefUsers)));
  owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  ReloadDeviceSettings();
  ASSERT_TRUE(service_->IsOwner());

  EXPECT_TRUE(FindInListValue(device_policy_->policy_data().username(),
                              provider_->Get(kAccountsPrefUsers)));
}

}  // namespace chromeos
