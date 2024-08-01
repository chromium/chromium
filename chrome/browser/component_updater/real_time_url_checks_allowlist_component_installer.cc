// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/component_updater/real_time_url_checks_allowlist_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/safe_browsing/android/real_time_url_checks_allowlist.h"

using component_updater::ComponentUpdateService;

namespace {

const base::FilePath::CharType kRealTimeUrlChecksAllowlistPbFileName[] =
    FILE_PATH_LITERAL("real_time_url_checks_allowlist.pb");
constexpr char kInstallerLoadFromDiskPbFileEmpty[] =
    "SafeBrowsing.Android.RealTimeAllowlist.InstallerLoadFromDiskPbFileEmpty";

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: aagaghndoahmfdbmfnajfklaomcanlnh
const uint8_t kRealTimeUrlChecksAllowlistPublicKeySHA256[32] = {
    0x00, 0x60, 0x67, 0xd3, 0xe0, 0x7c, 0x53, 0x1c, 0x5d, 0x09, 0x5a,
    0xb0, 0xec, 0x20, 0xdb, 0xd7, 0x5b, 0x15, 0xa2, 0x3c, 0x21, 0xe9,
    0xea, 0xde, 0x48, 0x11, 0x7a, 0x50, 0x43, 0x6c, 0xe9, 0x45};

const char kRealTimeUrlChecksAllowlistManifestName[] =
    "Real Time Url Checks Allowlist";

void LoadFromDisk(const base::FilePath& pb_path) {
  base::UmaHistogramBoolean(kInstallerLoadFromDiskPbFileEmpty, pb_path.empty());
  if (pb_path.empty()) {
    return;
  }

  std::string binary_pb;
  if (!base::ReadFileToString(pb_path, &binary_pb)) {
    binary_pb.clear();
  }

  safe_browsing::RealTimeUrlChecksAllowlist::GetInstance()
      ->PopulateFromDynamicUpdate(binary_pb);
}

}  // namespace

namespace component_updater {

bool RealTimeUrlChecksAllowlistComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool RealTimeUrlChecksAllowlistComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
RealTimeUrlChecksAllowlistComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void RealTimeUrlChecksAllowlistComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath
RealTimeUrlChecksAllowlistComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kRealTimeUrlChecksAllowlistPbFileName);
}

void RealTimeUrlChecksAllowlistComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadFromDisk, GetInstalledPath(install_dir)));
}

// Called during startup and installation before ComponentReady().
bool RealTimeUrlChecksAllowlistComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the proto here, since we'll do the checking
  // in |PopulateFromDynamicUpdate()|.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
RealTimeUrlChecksAllowlistComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("RealTimeUrlChecksAllowlist"));
}

void RealTimeUrlChecksAllowlistComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kRealTimeUrlChecksAllowlistPublicKeySHA256,
               kRealTimeUrlChecksAllowlistPublicKeySHA256 +
                   std::size(kRealTimeUrlChecksAllowlistPublicKeySHA256));
}

std::string RealTimeUrlChecksAllowlistComponentInstallerPolicy::GetName()
    const {
  return kRealTimeUrlChecksAllowlistManifestName;
}

update_client::InstallerAttributes
RealTimeUrlChecksAllowlistComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

void RegisterRealTimeUrlChecksAllowlistComponent(ComponentUpdateService* cus) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<RealTimeUrlChecksAllowlistComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
