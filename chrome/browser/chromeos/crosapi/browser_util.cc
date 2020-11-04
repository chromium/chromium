// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/browser_util.h"

#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/exo/shell_surface_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

using user_manager::User;
using version_info::Channel;

namespace crosapi {
namespace browser_util {
namespace {

// When this feature is enabled, Lacros will be available on stable channel.
const base::Feature kLacrosAllowOnStableChannel{
    "LacrosAllowOnStableChannel", base::FEATURE_DISABLED_BY_DEFAULT};

// Some account types require features that aren't yet supported by lacros.
// See https://crbug.com/1080693
bool IsUserTypeAllowed(const User* user) {
  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return true;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_SUPERVISED:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_CHILD:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
    case user_manager::NUM_USER_TYPES:
      return false;
  }
}

using InterfaceVersions = base::flat_map<base::Token, uint32_t>;
template <typename T>
void AddVersion(InterfaceVersions* map) {
  (*map)[T::Uuid_] = T::Version_;
}

mojom::LacrosInitParamsPtr GetLacrosInitParams(
    EnvironmentProvider* environment_provider) {
  auto params = mojom::LacrosInitParams::New();
  params->ash_chrome_service_version =
      crosapi::mojom::AshChromeService::Version_;
  params->deprecated_ash_metrics_enabled_has_value = true;
  params->ash_metrics_enabled = g_browser_process->local_state()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled);

  params->session_type = environment_provider->GetSessionType();
  params->device_mode = environment_provider->GetDeviceMode();
  params->interface_versions = GetInterfaceVersions();

  return params;
}

}  // namespace

const char kLaunchOnLoginPref[] = "lacros.launch_on_login";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kLaunchOnLoginPref, /*default_value=*/false);
}

base::FilePath GetUserDataDir() {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // NOTE: On device this function is privacy/security sensitive. The
    // directory must be inside the encrypted user partition.
    return base::FilePath(crosapi::kLacrosUserDataPath);
  }
  // For developers on Linux desktop, put the directory under the developer's
  // specified --user-data-dir.
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &base_path);
  return base_path.Append("lacros");
}

bool IsLacrosAllowed() {
  return IsLacrosAllowed(chrome::GetChannel());
}

bool IsLacrosAllowed(Channel channel) {
  const User* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return false;

  if (!IsUserTypeAllowed(user))
    return false;

  // TODO(https://crbug.com/1135494): Remove the free ticket for
  // Channel::UNKNOWN after the policy is set on server side for developers.
  if (channel == Channel::UNKNOWN)
    return true;

  if (!g_browser_process->local_state()->GetBoolean(prefs::kLacrosAllowed))
    return false;

  switch (channel) {
    case Channel::UNKNOWN:
    case Channel::CANARY:
    case Channel::DEV:
    case Channel::BETA:
      // Canary/dev/beta builds can use Lacros.
      // Developer builds can use lacros.
      return true;
    case Channel::STABLE:
      return base::FeatureList::IsEnabled(kLacrosAllowOnStableChannel);
  }
}

bool IsLacrosWindow(const aura::Window* window) {
  const std::string* app_id = exo::GetShellApplicationId(window);
  if (!app_id)
    return false;
  return base::StartsWith(*app_id, kLacrosAppIdPrefix);
}

base::flat_map<base::Token, uint32_t> GetInterfaceVersions() {
  static_assert(
      crosapi::mojom::AshChromeService::Version_ == 5,
      "if you add a new crosapi, please add it to the version map here");
  InterfaceVersions versions;
  AddVersion<crosapi::mojom::AccountManager>(&versions);
  AddVersion<crosapi::mojom::AshChromeService>(&versions);
  AddVersion<crosapi::mojom::Feedback>(&versions);
  AddVersion<crosapi::mojom::FileManager>(&versions);
  AddVersion<crosapi::mojom::KeystoreService>(&versions);
  AddVersion<crosapi::mojom::MessageCenter>(&versions);
  AddVersion<crosapi::mojom::ScreenManager>(&versions);
  AddVersion<crosapi::mojom::SnapshotCapturer>(&versions);
  AddVersion<device::mojom::HidConnection>(&versions);
  AddVersion<device::mojom::HidManager>(&versions);
  return versions;
}

mojo::Remote<crosapi::mojom::LacrosChromeService>
SendMojoInvitationToLacrosChrome(
    EnvironmentProvider* environment_provider,
    mojo::PlatformChannelEndpoint local_endpoint,
    base::OnceClosure mojo_disconnected_callback,
    base::OnceCallback<
        void(mojo::PendingReceiver<crosapi::mojom::AshChromeService>)>
        ash_chrome_service_callback) {
  mojo::OutgoingInvitation invitation;
  mojo::Remote<crosapi::mojom::LacrosChromeService> lacros_chrome_service;
  lacros_chrome_service.Bind(
      mojo::PendingRemote<crosapi::mojom::LacrosChromeService>(
          invitation.AttachMessagePipe(0 /* token */), /*version=*/0));
  lacros_chrome_service.set_disconnect_handler(
      std::move(mojo_disconnected_callback));
  lacros_chrome_service->Init(GetLacrosInitParams(environment_provider));
  lacros_chrome_service->RequestAshChromeServiceReceiver(
      std::move(ash_chrome_service_callback));
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 std::move(local_endpoint));
  return lacros_chrome_service;
}

}  // namespace browser_util
}  // namespace crosapi
