// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/recovery_improved_component_installer.h"

#include <iterator>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wrl/client.h>
#include "chrome/install_static/install_util.h"
#endif

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/elevation_service/elevation_service_idl.h"
#endif

// This component is behind a Finch experiment. To enable the registration of
// the component, run Chrome with --enable-features=ImprovedRecoveryComponent.
namespace component_updater {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component CRX.
// The component id is: ihnlcenocehgdaegdmhbidjhnhdchfmm
constexpr uint8_t kRecoveryImprovedPublicKeySHA256[32] = {
    0x87, 0xdb, 0x24, 0xde, 0x24, 0x76, 0x30, 0x46, 0x3c, 0x71, 0x83,
    0x97, 0xd7, 0x32, 0x75, 0xcc, 0xd5, 0x7f, 0xec, 0x09, 0x60, 0x6d,
    0x20, 0xc3, 0x81, 0xd7, 0xce, 0x7b, 0x10, 0x15, 0x44, 0xd1};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_WIN)
// Instantiates the elevator service, calls its elevator interface, then
// blocks waiting for the recovery processes to exit. Returns the result
// of the recovery as a tuple.
std::tuple<bool, int, int> RunRecoveryCRXElevated(
    const base::FilePath& crx_path,
    const std::string& browser_appid,
    const std::string& browser_version,
    const std::string& session_id) {
  Microsoft::WRL::ComPtr<IElevator> elevator;
  HRESULT hr = CoCreateInstance(
      install_static::GetElevatorClsid(), nullptr, CLSCTX_LOCAL_SERVER,
      install_static::GetElevatorIid(), IID_PPV_ARGS_Helper(&elevator));
  if (FAILED(hr))
    return {false, static_cast<int>(hr), 0};

  hr = CoSetProxyBlanket(
      elevator.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_DYNAMIC_CLOAKING);
  if (FAILED(hr))
    return {false, static_cast<int>(hr), 0};

  ULONG_PTR proc_handle = 0;
  hr = elevator->RunRecoveryCRXElevated(
      crx_path.value().c_str(), base::SysUTF8ToWide(browser_appid).c_str(),
      base::SysUTF8ToWide(browser_version).c_str(),
      base::SysUTF8ToWide(session_id).c_str(), base::Process::Current().Pid(),
      &proc_handle);
  if (FAILED(hr))
    return {false, static_cast<int>(hr), 0};

  int exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  base::Process process(reinterpret_cast<base::ProcessHandle>(proc_handle));
  const bool succeeded =
      process.WaitForExitWithTimeout(kMaxWaitTime, &exit_code);
  return {succeeded, exit_code, 0};
}
#endif

RecoveryImprovedInstallerPolicy::RecoveryImprovedInstallerPolicy(
    PrefService* prefs)
    : prefs_(prefs) {}

RecoveryImprovedInstallerPolicy::~RecoveryImprovedInstallerPolicy() {}

bool RecoveryImprovedInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool RecoveryImprovedInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
RecoveryImprovedInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void RecoveryImprovedInstallerPolicy::OnCustomUninstall() {}

void RecoveryImprovedInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DVLOG(1) << "RecoveryImproved component is ready.";
}

// Called during startup and installation before ComponentReady().
bool RecoveryImprovedInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath RecoveryImprovedInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("RecoveryImproved"));
}

void RecoveryImprovedInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kRecoveryImprovedPublicKeySHA256),
               std::end(kRecoveryImprovedPublicKeySHA256));
}

std::string RecoveryImprovedInstallerPolicy::GetName() const {
  return "Chrome Improved Recovery";
}

update_client::InstallerAttributes
RecoveryImprovedInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> RecoveryImprovedInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterRecoveryImprovedComponent(ComponentUpdateService* cus,
                                       PrefService* prefs) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if defined(OS_WIN) || defined(OS_MACOSX)
  DVLOG(1) << "Registering RecoveryImproved component.";

  // |cus| takes ownership of |installer| through the CrxComponent instance.
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<RecoveryImprovedInstallerPolicy>(prefs));
  installer->Register(cus, base::OnceClosure());
#endif
#endif
}

void RegisterPrefsForRecoveryImprovedComponent(PrefRegistrySimple* registry) {}

}  // namespace component_updater
