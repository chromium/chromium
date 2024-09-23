// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/login/test/device_state_mixin.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_paths.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/install_attributes.pb.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

constexpr char kFakeDomain[] = "example.com";
constexpr char kFakeDeviceId[] = "device_id";

bool g_instance_created = false;

cryptohome::SerializedInstallAttributes BuildInstallAttributes(
    const std::string& mode,
    const std::string& domain,
    const std::string& realm,
    const std::string& device_id) {
  std::map<std::string, std::string> install_attrs_;
  install_attrs_["enterprise.mode"] = mode;
  install_attrs_["enterprise.domain"] = domain;
  install_attrs_["enterprise.realm"] = realm;
  install_attrs_["enterprise.device_id"] = device_id;
  if (!mode.empty())
    install_attrs_["enterprise.owned"] = "true";

  cryptohome::SerializedInstallAttributes install_attrs;
  install_attrs.set_version(1);

  for (const auto& it : install_attrs_) {
    if (it.second.empty())
      continue;
    cryptohome::SerializedInstallAttributes::Attribute* attr_entry =
        install_attrs.add_attributes();
    const std::string& name = it.first;
    const std::string& value = it.second;
    attr_entry->set_name(name);
    attr_entry->mutable_value()->assign(value.data(),
                                        value.data() + value.size());
  }
  return install_attrs;
}

void WriteFile(const base::FilePath& path, const std::string& blob) {
  CHECK(base::WriteFile(path, blob));
}

}  // namespace

DeviceStateMixin::DeviceStateMixin(InProcessBrowserTestMixinHost* host,
                                   State initial_state)
    : InProcessBrowserTestMixin(host),
      state_(initial_state),
      local_state_mixin_(host, this) {
  DCHECK(!g_instance_created);
  g_instance_created = true;
}

bool DeviceStateMixin::SetUpUserDataDirectory() {
  SetDeviceState();
  return true;
}

void DeviceStateMixin::SetUpInProcessBrowserTestFixture() {
  // Make sure session manager client has been initialized as in-memory. This is
  // requirement for setting policy blobs.
  if (!SessionManagerClient::Get())
    SessionManagerClient::InitializeFakeInMemory();

  session_manager_initialized_ = true;

  std::vector<std::string> state_keys;
  state_keys.push_back("1");
  FakeSessionManagerClient::Get()->set_server_backed_state_keys(state_keys);

  if (IsEnrolledState() && !skip_initial_policy_setup_) {
    SetCachedDevicePolicy();

    for (const auto& device_local_account : device_local_account_policies_)
      SetCachedDeviceLocalAccountPolicy(device_local_account.first);
  }
}

void DeviceStateMixin::SetUpLocalState() {
  PrefService* local_state = g_browser_process->local_state();
  switch (state_) {
    case DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED:
    case DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED:
    case DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED:
    case DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE:
      local_state->SetBoolean(prefs::kOobeComplete, true);
      local_state->SetInteger(::prefs::kDeviceRegistered, 1);
      local_state->SetBoolean(::prefs::kEnrollmentRecoveryRequired, false);
      break;
    case DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED:
    case DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED:
      local_state->SetBoolean(prefs::kOobeComplete, true);
      local_state->SetInteger(::prefs::kDeviceRegistered, 0);
      local_state->SetBoolean(::prefs::kEnrollmentRecoveryRequired, false);
      break;
    case DeviceStateMixin::State::BEFORE_OOBE:
      local_state->SetInteger(::prefs::kDeviceRegistered, 0);
      break;
  }
}

std::unique_ptr<ScopedDevicePolicyUpdate>
DeviceStateMixin::RequestDevicePolicyUpdate() {
  if (!IsEnrolledState())
    return nullptr;

  return std::make_unique<ScopedDevicePolicyUpdate>(
      &device_policy_, base::BindOnce(&DeviceStateMixin::SetCachedDevicePolicy,
                                      weak_factory_.GetWeakPtr()));
}

std::unique_ptr<ScopedUserPolicyUpdate>
DeviceStateMixin::RequestDeviceLocalAccountPolicyUpdate(
    const std::string& account_id) {
  if (!IsEnrolledState())
    return nullptr;

  policy::UserPolicyBuilder& builder =
      device_local_account_policies_[account_id];
  return std::make_unique<ScopedUserPolicyUpdate>(
      &builder,
      base::BindRepeating(&DeviceStateMixin::SetCachedDeviceLocalAccountPolicy,
                          weak_factory_.GetWeakPtr(), account_id));
}

void DeviceStateMixin::SetState(State state) {
  DCHECK(!is_setup_) << "SetState called after device was set up";
  state_ = state;
}

void DeviceStateMixin::SetDeviceState() {
  DCHECK(!is_setup_);
  DCHECK(domain_.empty() || state_ == State::OOBE_COMPLETED_CLOUD_ENROLLED);
  is_setup_ = true;

  WriteInstallAttrFile(state_);
  WriteOwnerKey();
}

void DeviceStateMixin::WriteInstallAttrFile(State state) {
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::FilePath install_attrs_file =
      user_data_dir.Append("stub_install_attributes.pb");

  base::ScopedAllowBlockingForTesting allow_blocking;
  if (base::PathExists(install_attrs_file)) {
    return;
  }

  std::string device_mode, domain, realm, device_id;
  switch (state) {
    case DeviceStateMixin::State::BEFORE_OOBE:
    case DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED:
      // No file at all.
      return;
    case DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED:
      // File with version only. This will prevent Chrome from attempting to
      // take consumer ownership and automatically advancing to the next state.
    case DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED:
      // File with version only.
      break;
    case DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED:
      device_mode = "enterprise";
      domain = !domain_.empty() ? domain_ : kFakeDomain;
      device_id = kFakeDeviceId;
      break;
    case DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED:
      device_mode = "enterprise_ad";
      realm = kFakeDomain;
      device_id = kFakeDeviceId;
      break;
    case DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE:
      device_mode = "demo_mode";
      domain = "cros-demo-mode.com";
      device_id = kFakeDeviceId;
      break;
  }

  std::string install_attrs_bits;
  CHECK(BuildInstallAttributes(device_mode, domain, realm, device_id)
            .SerializeToString(&install_attrs_bits));
  WriteFile(install_attrs_file, install_attrs_bits);
}

void DeviceStateMixin::WriteOwnerKey() {
  switch (state_) {
    case DeviceStateMixin::State::BEFORE_OOBE:
    case DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED:
    case DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED:
    case DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED:
      return;
    case DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED:
    case DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED:
    case DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE:
      break;
  }

  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::FilePath owner_key_file = user_data_dir.Append("stub_owner.key");
  const std::string owner_key_bits =
      policy::PolicyBuilder::GetPublicTestKeyAsString();
  CHECK(!owner_key_bits.empty());
  WriteFile(owner_key_file, owner_key_bits);
}

bool DeviceStateMixin::IsEnrolledState() const {
  switch (state_) {
    case DeviceStateMixin::State::BEFORE_OOBE:
    case DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED:
    case DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED:
    case DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED:
      return false;
    case DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED:
    case DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED:
    case DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE:
      return true;
  }
  return false;
}

void DeviceStateMixin::SetCachedDevicePolicy() {
  if (!session_manager_initialized_)
    return;

  DCHECK(IsEnrolledState());

  device_policy_.SetDefaultSigningKey();
  device_policy_.Build();
  FakeSessionManagerClient::Get()->set_device_policy(device_policy_.GetBlob());
  FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
}

void DeviceStateMixin::SetCachedDeviceLocalAccountPolicy(
    const std::string& account_id) {
  if (!session_manager_initialized_ ||
      !device_local_account_policies_.count(account_id))
    return;

  DCHECK(IsEnrolledState());

  policy::UserPolicyBuilder& builder =
      device_local_account_policies_[account_id];
  builder.policy_data().set_username(account_id);
  builder.policy_data().set_settings_entity_id(account_id);
  builder.policy_data().set_policy_type(
      policy::dm_protocol::kChromePublicAccountPolicyType);
  builder.SetDefaultSigningKey();
  builder.Build();

  FakeSessionManagerClient::Get()->set_device_local_account_policy(
      account_id, builder.GetBlob());
}

DeviceStateMixin::~DeviceStateMixin() = default;

}  // namespace ash
