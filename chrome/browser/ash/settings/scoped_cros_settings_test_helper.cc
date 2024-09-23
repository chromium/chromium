// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"

#include <memory>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

ScopedCrosSettingsTestHelper::ScopedCrosSettingsTestHelper(
    bool create_settings_service)
    : stub_settings_provider_(std::make_unique<StubCrosSettingsProvider>()),
      stub_settings_provider_ptr_(static_cast<StubCrosSettingsProvider*>(
          stub_settings_provider_.get())) {
  Initialize(create_settings_service);
}

ScopedCrosSettingsTestHelper::~ScopedCrosSettingsTestHelper() {
  RestoreRealDeviceSettingsProvider();
}

std::unique_ptr<FakeOwnerSettingsService>
ScopedCrosSettingsTestHelper::CreateOwnerSettingsService(Profile* profile) {
  return std::make_unique<FakeOwnerSettingsService>(
      stub_settings_provider_ptr_, profile, new ownership::MockOwnerKeyUtil());
}

void ScopedCrosSettingsTestHelper::ReplaceDeviceSettingsProviderWithStub() {
  CHECK(CrosSettings::IsInitialized());
  // This function shouldn't be called twice.
  CHECK(!real_settings_provider_);

  CrosSettings* const cros_settings = CrosSettings::Get();

  // TODO(olsen): This could be simplified if DeviceSettings and CrosSettings
  // were the same thing, which they nearly are, except for 3 timezone settings.
  CrosSettingsProvider* real_settings_provider =
      cros_settings->GetProvider(kDeviceOwner);  // Can be any device setting.
  EXPECT_TRUE(real_settings_provider);
  real_settings_provider_ =
      cros_settings->RemoveSettingsProvider(real_settings_provider);
  EXPECT_TRUE(real_settings_provider_);
  cros_settings->AddSettingsProvider(std::move(stub_settings_provider_));
}

void ScopedCrosSettingsTestHelper::RestoreRealDeviceSettingsProvider() {
  if (real_settings_provider_) {
    // Restore the real DeviceSettingsProvider.
    CrosSettings* const cros_settings = CrosSettings::Get();
    stub_settings_provider_ =
        cros_settings->RemoveSettingsProvider(stub_settings_provider_ptr_);
    EXPECT_TRUE(stub_settings_provider_);
    cros_settings->AddSettingsProvider(std::move(real_settings_provider_));
  }
  real_settings_provider_.reset();
}

StubCrosSettingsProvider* ScopedCrosSettingsTestHelper::GetStubbedProvider() {
  return stub_settings_provider_ptr_;
}

void ScopedCrosSettingsTestHelper::SetTrustedStatus(
    CrosSettingsProvider::TrustedStatus status) {
  GetStubbedProvider()->SetTrustedStatus(status);
}

void ScopedCrosSettingsTestHelper::SetCurrentUserIsOwner(bool owner) {
  GetStubbedProvider()->SetCurrentUserIsOwner(owner);
}

void ScopedCrosSettingsTestHelper::Set(const std::string& path,
                                       const base::Value& in_value) {
  GetStubbedProvider()->Set(path, in_value);
}

void ScopedCrosSettingsTestHelper::SetBoolean(const std::string& path,
                                              bool in_value) {
  Set(path, base::Value(in_value));
}

void ScopedCrosSettingsTestHelper::SetInteger(const std::string& path,
                                              int in_value) {
  Set(path, base::Value(in_value));
}

void ScopedCrosSettingsTestHelper::SetDouble(const std::string& path,
                                             double in_value) {
  Set(path, base::Value(in_value));
}

void ScopedCrosSettingsTestHelper::SetString(const std::string& path,
                                             const std::string& in_value) {
  Set(path, base::Value(in_value));
}

void ScopedCrosSettingsTestHelper::StoreCachedDeviceSetting(
    const std::string& path) {
  const base::Value* const value = stub_settings_provider_ptr_->Get(path);
  if (value) {
    enterprise_management::PolicyData data;
    enterprise_management::ChromeDeviceSettingsProto settings;
    if (device_settings_cache::Retrieve(&data,
                                        g_browser_process->local_state())) {
      CHECK(settings.ParseFromString(data.policy_value()));
    }
    OwnerSettingsServiceAsh::UpdateDeviceSettings(path, *value, settings);
    CHECK(settings.SerializeToString(data.mutable_policy_value()));
    CHECK(device_settings_cache::Store(data, g_browser_process->local_state()));
    g_browser_process->local_state()->CommitPendingWrite(base::DoNothing(),
                                                         base::DoNothing());
  }
}

void ScopedCrosSettingsTestHelper::CopyStoredValue(const std::string& path) {
  CrosSettingsProvider* provider = real_settings_provider_
                                       ? real_settings_provider_.get()
                                       : CrosSettings::Get()->GetProvider(path);
  const base::Value* const value = provider->Get(path);
  if (value) {
    stub_settings_provider_ptr_->Set(path, *value);
  }
}

StubInstallAttributes* ScopedCrosSettingsTestHelper::InstallAttributes() {
  return test_install_attributes_->Get();
}

void ScopedCrosSettingsTestHelper::Initialize(bool create_settings_service) {
  if (create_settings_service) {
    test_install_attributes_ = std::make_unique<ScopedStubInstallAttributes>();
    CHECK(!DeviceSettingsService::IsInitialized());
    test_device_settings_service_ =
        std::make_unique<ScopedTestDeviceSettingsService>();
    cros_settings_holder_ = std::make_unique<CrosSettingsHolder>(
        ash::DeviceSettingsService::Get(), g_browser_process->local_state());
  }
}

}  // namespace ash
