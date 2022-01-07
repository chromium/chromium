// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/first_party_sets_component_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/features.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/network_service.mojom.h"

using component_updater::ComponentUpdateService;

namespace {

using SetsReadyOnceCallback = component_updater::
    FirstPartySetsComponentInstallerPolicy::SetsReadyOnceCallback;

constexpr base::FilePath::CharType kFirstPartySetsSetsFileName[] =
    FILE_PATH_LITERAL("sets.json");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: gonpemdgkjcecdgbnaabipppbmgfggbe
constexpr uint8_t kFirstPartySetsPublicKeySHA256[32] = {
    0x6e, 0xdf, 0x4c, 0x36, 0xa9, 0x24, 0x23, 0x61, 0xd0, 0x01, 0x8f,
    0xff, 0x1c, 0x65, 0x66, 0x14, 0xa8, 0x46, 0x37, 0xe6, 0xeb, 0x80,
    0x8b, 0x8f, 0xb0, 0xb6, 0x18, 0xa7, 0xcd, 0x3d, 0xbb, 0xfb};

constexpr char kFirstPartySetsManifestName[] = "First-Party Sets";

constexpr base::FilePath::CharType kFirstPartySetsRelativeInstallDir[] =
    FILE_PATH_LITERAL("FirstPartySetsPreloaded");

base::File OpenFile(const base::FilePath& pb_path) {
  return base::File(pb_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

base::FilePath& GetConfigPathInstance() {
  static base::NoDestructor<base::FilePath> instance;
  return *instance;
}

// Determine if First-Party Sets is enabled by checking both the feature
// and the enterprise policy.
bool IsFirstPartySetsEnabled() {
  // TODO(https://crbug.com/1269360): move this logic into
  // FirstPartySetsUtil
  if (!base::FeatureList::IsEnabled(net::features::kFirstPartySets)) {
    return false;
  }
  // Some java tests start the browser in minimal mode, in which
  // `g_browser_process` is unset.
  if (!g_browser_process)
    return false;
  auto* local_state = g_browser_process->local_state();
  if (!local_state ||
      !local_state->HasPrefPath(first_party_sets::kFirstPartySetsEnabled)) {
    return true;
  }
  return local_state->GetBoolean(first_party_sets::kFirstPartySetsEnabled);
}

// Invokes `on_sets_ready`, if:
// * First-Party Sets is enabled; and
// * `on_sets_ready` is not null.
//
// If the component has been installed and can be read, we pass the component
// file; otherwise, we pass an invalid file.
void SetFirstPartySetsConfig(SetsReadyOnceCallback on_sets_ready) {
  if (!IsFirstPartySetsEnabled() || on_sets_ready.is_null()) {
    return;
  }

  const base::FilePath instance_path = GetConfigPathInstance();
  if (instance_path.empty()) {
    // Registration is complete, but no component version exists on disk.
    std::move(on_sets_ready).Run(base::File());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&OpenFile, instance_path), std::move(on_sets_ready));
}

std::string BoolToString(bool b) {
  return b ? "true" : "false";
}

}  // namespace

namespace component_updater {

// static
void FirstPartySetsComponentInstallerPolicy::ReconfigureAfterNetworkRestart(
    SetsReadyOnceCallback on_sets_ready) {
  SetFirstPartySetsConfig(std::move(on_sets_ready));
}

void FirstPartySetsComponentInstallerPolicy::OnRegistrationComplete() {
  SetFirstPartySetsConfig(std::move(on_sets_ready_));
}

FirstPartySetsComponentInstallerPolicy::FirstPartySetsComponentInstallerPolicy(
    SetsReadyOnceCallback on_sets_ready)
    : on_sets_ready_(std::move(on_sets_ready)) {}

FirstPartySetsComponentInstallerPolicy::
    ~FirstPartySetsComponentInstallerPolicy() = default;

const char
    FirstPartySetsComponentInstallerPolicy::kDogfoodInstallerAttributeName[] =
        "_internal_experimental_sets";

const char FirstPartySetsComponentInstallerPolicy::kV2FormatOptIn[] =
    "_v2_format_plz";

bool FirstPartySetsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool FirstPartySetsComponentInstallerPolicy::RequiresNetworkEncryption() const {
  // Update checks and pings associated with this component do not require
  // confidentiality, since the component is identical for all users.
  return false;
}

update_client::CrxInstaller::Result
FirstPartySetsComponentInstallerPolicy::OnCustomInstall(
    const base::Value& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void FirstPartySetsComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kFirstPartySetsSetsFileName);
}

void FirstPartySetsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value manifest) {
  if (install_dir.empty() || !GetConfigPathInstance().empty())
    return;

  VLOG(1) << "First-Party Sets Component ready, version " << version.GetString()
          << " in " << install_dir.value();

  GetConfigPathInstance() = GetInstalledPath(install_dir);

  SetFirstPartySetsConfig(std::move(on_sets_ready_));
}

// Called during startup and installation before ComponentReady().
bool FirstPartySetsComponentInstallerPolicy::VerifyInstallation(
    const base::Value& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the sets here, since we'll do the validation
  // in the Network Service.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath FirstPartySetsComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kFirstPartySetsRelativeInstallDir);
}

void FirstPartySetsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kFirstPartySetsPublicKeySHA256,
               kFirstPartySetsPublicKeySHA256 +
                   base::size(kFirstPartySetsPublicKeySHA256));
}

std::string FirstPartySetsComponentInstallerPolicy::GetName() const {
  return kFirstPartySetsManifestName;
}

update_client::InstallerAttributes
FirstPartySetsComponentInstallerPolicy::GetInstallerAttributes() const {
  return {
      {
          kDogfoodInstallerAttributeName,
          BoolToString(net::features::kFirstPartySetsIsDogfooder.Get()),
      },
      {
          kV2FormatOptIn,
          BoolToString(base::FeatureList::IsEnabled(
              net::features::kFirstPartySetsV2ComponentFormat)),
      },
  };
}

// static
void FirstPartySetsComponentInstallerPolicy::ResetForTesting() {
  GetConfigPathInstance().clear();
}

void RegisterFirstPartySetsComponent(ComponentUpdateService* cus) {
  VLOG(1) << "Registering First-Party Sets component.";

  auto policy = std::make_unique<FirstPartySetsComponentInstallerPolicy>(
      /*on_sets_ready=*/base::BindOnce([](base::File sets_file) {
        VLOG(1) << "Received First-Party Sets";
        content::GetNetworkService()->SetFirstPartySets(std::move(sets_file));
      }));

  FirstPartySetsComponentInstallerPolicy* raw_policy = policy.get();
  // Dereferencing `raw_policy` this way is safe because the closure is invoked
  // by the ComponentInstaller instance, which owns `policy` (so they have the
  // same lifetime). Therefore if/when the closure is invoked, `policy` is still
  // alive.
  base::MakeRefCounted<ComponentInstaller>(std::move(policy))
      ->Register(cus, base::BindOnce(
                          [](FirstPartySetsComponentInstallerPolicy* policy) {
                            policy->OnRegistrationComplete();
                          },
                          raw_policy));
}

// static
void FirstPartySetsComponentInstallerPolicy::WriteComponentForTesting(
    const base::FilePath& install_dir,
    base::StringPiece contents) {
  CHECK(base::WriteFile(GetInstalledPath(install_dir), contents));

  GetConfigPathInstance() = GetInstalledPath(install_dir);
}

}  // namespace component_updater
