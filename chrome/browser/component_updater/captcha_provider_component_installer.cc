// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/captcha_provider_component_installer.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"
#include "components/component_updater/component_updater_service.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The extension id is: pficcddpglkpaaihklmahepgjmefdnom
constexpr uint8_t kCaptchaProviderPublicKeySha256[32] = {
    0xf5, 0x82, 0x23, 0x3f, 0x6b, 0xaf, 0x00, 0x87, 0xab, 0xc0, 0x74,
    0xf6, 0x9c, 0x45, 0x3d, 0xec, 0xbf, 0x0a, 0xb2, 0x59, 0x2a, 0x9c,
    0xfc, 0xb4, 0x58, 0x84, 0xcb, 0x22, 0xb7, 0xb8, 0xbe, 0xbc};

constexpr char kCaptchaProviderManifestName[] = "Probabilistic Reveal Tokens";

constexpr base::FilePath::CharType kCaptchaProviderJsonFileName[] =
    FILE_PATH_LITERAL("captcha_providers.json");

constexpr base::FilePath::CharType kCaptchaProviderRelativeInstallDir[] =
    FILE_PATH_LITERAL("CaptchaProviders");

std::optional<std::string> LoadCaptchaProviderJsonFromDisk(
    const base::FilePath& json_path) {
  CHECK(!json_path.empty());

  VLOG(1) << "Reading Captcha Providers from file: " << json_path.value();
  std::string json_content;
  if (!base::ReadFileToString(json_path, &json_content)) {
    VLOG(1) << "Failed reading from " << json_path.value();
    return std::nullopt;
  }
  return json_content;
}

}  // namespace

namespace component_updater {

CaptchaProviderComponentInstallerPolicy::
    CaptchaProviderComponentInstallerPolicy(
        CaptchaProvidersReadyRepeatingCallback on_ready)
    : on_ready_(std::move(on_ready)) {}

CaptchaProviderComponentInstallerPolicy::
    CaptchaProviderComponentInstallerPolicy() = default;

CaptchaProviderComponentInstallerPolicy::
    ~CaptchaProviderComponentInstallerPolicy() = default;

bool CaptchaProviderComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool CaptchaProviderComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
CaptchaProviderComponentInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void CaptchaProviderComponentInstallerPolicy::OnCustomUninstall() {}

void CaptchaProviderComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  VLOG(1) << "Captcha Providers Component ready, version "
          << version.GetString() << " in " << install_dir.value();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LoadCaptchaProviderJsonFromDisk,
                     GetInstalledPath(install_dir)),
      base::BindOnce(on_ready_));
}

// Called during startup and installation before ComponentReady().
bool CaptchaProviderComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath CaptchaProviderComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kCaptchaProviderRelativeInstallDir);
}

void CaptchaProviderComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kCaptchaProviderPublicKeySha256),
               std::end(kCaptchaProviderPublicKeySha256));
}

std::string CaptchaProviderComponentInstallerPolicy::GetName() const {
  return kCaptchaProviderManifestName;
}

update_client::InstallerAttributes
CaptchaProviderComponentInstallerPolicy::GetInstallerAttributes() const {
  return {};
}

// static
base::FilePath CaptchaProviderComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& install_dir) {
  return install_dir.Append(kCaptchaProviderJsonFileName);
}

void OnCaptchaProviderComponentReady(std::optional<std::string> json_content) {
  if (!json_content.has_value()) {
    VLOG(1) << "Failed to receive Captcha Providers.";
    return;
  }
  VLOG(1) << "Received Captcha Providers.";

  std::optional<base::Value> json = base::JSONReader::Read(
      json_content.value(), base::JSON_ALLOW_TRAILING_COMMAS);
  if (!json.has_value()) {
    VLOG(1) << "Failed to parse Captcha Providers json.";
    return;
  }

  base::ListValue* json_list = json->GetIfList();
  if (!json_list) {
    VLOG(1) << "Failed to get top level Captcha Providers list.";
    return;
  }

  std::vector<std::string> captcha_providers;
  for (const auto& item : *json_list) {
    const std::string* provider = item.GetIfString();
    if (!provider) {
      VLOG(1) << "Failed to get Captcha Provider.";
      continue;
    }
    captcha_providers.push_back(*provider);
  }

  page_load_metrics::CaptchaProviderManager::GetInstance()->SetCaptchaProviders(
      captcha_providers);
}

void RegisterCaptchaProviderComponent(
    component_updater::ComponentUpdateService* cus) {
  VLOG(1) << "Registering Captcha Provider component.";

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<CaptchaProviderComponentInstallerPolicy>(
          /*on_ready=*/base::BindRepeating(OnCaptchaProviderComponentReady)));

  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
