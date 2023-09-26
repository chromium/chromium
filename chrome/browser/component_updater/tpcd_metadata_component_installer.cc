// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/tpcd_metadata_component_installer.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "components/component_updater/component_updater_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/tpcd/metadata/parser.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using component_updater::ComponentUpdateService;

namespace {
// This is similar to the display name at http://omaharelease/1915488/settings
// and
// http://google3/java/com/google/installer/releasemanager/Automation.java;l=1161;rcl=553816031
const char kTpcdMetadataManifestName[] =
    "Third-Party Cookie Deprecation Metadata";

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: jflhchccmppkfebkiaminageehmchikm
const uint8_t kTpcdMetadataPublicKeySHA256[32] = {
    0x95, 0xb7, 0x27, 0x22, 0xcf, 0xfa, 0x54, 0x1a, 0x80, 0xc8, 0xd0,
    0x64, 0x47, 0xc2, 0x78, 0xac, 0x61, 0x26, 0x43, 0xbf, 0x3a, 0x51,
    0x2e, 0xa6, 0xce, 0x00, 0x25, 0x7b, 0x6c, 0xc4, 0x4e, 0x39};

const base::FilePath::CharType kComponentFileName[] =
    FILE_PATH_LITERAL("metadata.pb");

const base::FilePath::CharType kRelInstallDirName[] =
    FILE_PATH_LITERAL("TpcdMetadata");

// Runs on a thread pool.
absl::optional<std::string> ReadComponentFromDisk(
    const base::FilePath& file_path) {
  VLOG(1) << "Reading TPCD Metadata from file: " << file_path.value();
  std::string contents;
  if (!base::ReadFileToString(file_path, &contents)) {
    VLOG(1) << "Failed reading from " << file_path.value();
    return absl::nullopt;
  }
  return contents;
}

const base::FilePath GetComponentPath(const base::FilePath& install_dir) {
  return install_dir.Append(kComponentFileName);
}
}  // namespace

namespace component_updater {
TpcdMetadataComponentInstaller::TpcdMetadataComponentInstaller(
    OnTpcdMetadataComponentReadyCallback on_component_ready_callback)
    : on_component_ready_callback_(on_component_ready_callback) {}

TpcdMetadataComponentInstaller::~TpcdMetadataComponentInstaller() = default;

// Start of ComponentInstallerPolicy overrides impl:
bool TpcdMetadataComponentInstaller::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool TpcdMetadataComponentInstaller::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
TpcdMetadataComponentInstaller::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void TpcdMetadataComponentInstaller::OnCustomUninstall() {}

void TpcdMetadataComponentInstaller::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(1) << "TPCD Metadata Component ready, version " << version.GetString()
          << " in " << install_dir.value();

  if (base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants)) {
    // Given `BEST_EFFORT` since we don't need to be USER_BLOCKING.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&ReadComponentFromDisk, GetComponentPath(install_dir)),
        base::BindOnce(
            [](OnTpcdMetadataComponentReadyCallback on_component_ready_callback,
               const absl::optional<std::string>& maybe_contents) {
              if (maybe_contents.has_value()) {
                on_component_ready_callback.Run(maybe_contents.value());
              }
            },
            on_component_ready_callback_));
  }
}

void WriteMetrics(TpcdMetadataInstallationResult result) {
  base::UmaHistogramEnumeration(
      "Navigation.TpcdMitigations.MetadataInstallationResult", result);
}

// Called during startup and installation before ComponentReady().
bool TpcdMetadataComponentInstaller::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  if (!base::PathExists(GetComponentPath(install_dir))) {
    WriteMetrics(TpcdMetadataInstallationResult::kMissingMetadataFile);
    return false;
  }

  std::string contents;
  if (!base::ReadFileToString(GetComponentPath(install_dir), &contents)) {
    WriteMetrics(TpcdMetadataInstallationResult::kReadingMetadataFileFailed);
    return false;
  }

  tpcd::metadata::Metadata metadata;
  if (!metadata.ParseFromString(contents)) {
    WriteMetrics(TpcdMetadataInstallationResult::kParsingToProtoFailed);
    return false;
  }

  for (const auto& me : metadata.metadata_entries()) {
    if (!me.has_primary_pattern_spec() ||
        !ContentSettingsPattern::FromString(me.primary_pattern_spec())
             .IsValid()) {
      WriteMetrics(TpcdMetadataInstallationResult::kErroneousSpec);
      return false;
    }

    if (!me.has_secondary_pattern_spec() ||
        !ContentSettingsPattern::FromString(me.secondary_pattern_spec())
             .IsValid()) {
      WriteMetrics(TpcdMetadataInstallationResult::kErroneousSpec);
      return false;
    }
  }

  WriteMetrics(TpcdMetadataInstallationResult::kSuccessful);
  return true;
}

base::FilePath TpcdMetadataComponentInstaller::GetRelativeInstallDir() const {
  return base::FilePath(kRelInstallDirName);
}

void TpcdMetadataComponentInstaller::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kTpcdMetadataPublicKeySHA256),
               std::end(kTpcdMetadataPublicKeySHA256));
}

std::string TpcdMetadataComponentInstaller::GetName() const {
  return kTpcdMetadataManifestName;
}

update_client::InstallerAttributes
TpcdMetadataComponentInstaller::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}
// End of ComponentInstallerPolicy overrides impl.

void RegisterTpcdMetadataComponent(ComponentUpdateService* cus) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VLOG(1) << "Third Party Cookie Deprecation Metadata component.";

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TpcdMetadataComponentInstaller>(
          base::BindRepeating([](std::string raw_metadata) {
            tpcd::metadata::Parser::GetInstance()->ParseMetadata(raw_metadata);
          })));

  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
