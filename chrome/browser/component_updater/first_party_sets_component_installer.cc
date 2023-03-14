// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/first_party_sets_component_installer.h"

#include <utility>

#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "net/cookies/cookie_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

absl::optional<std::pair<base::FilePath, base::Version>>&
GetConfigPathInstance() {
  // Contains nullopt until registration is complete. Afterward, contains the
  // FilePath and version for the component file, or empty FilePath and version
  // if no component was installed at startup.
  static base::NoDestructor<
      absl::optional<std::pair<base::FilePath, base::Version>>>
      instance;
  return *instance;
}

base::TaskPriority GetTaskPriority() {
  return content::FirstPartySetsHandler::GetInstance()->IsEnabled()
             ? base::TaskPriority::USER_BLOCKING
             : base::TaskPriority::BEST_EFFORT;
}

// Invokes `on_sets_ready`, if:
// * First-Party Sets is enabled; and
// * `on_sets_ready` is not null.
//
// If the component has been installed and can be read, we pass the component
// file; otherwise, we pass an invalid file.
void SetFirstPartySetsConfig(SetsReadyOnceCallback on_sets_ready) {
  if (!content::FirstPartySetsHandler::GetInstance()->IsEnabled() ||
      on_sets_ready.is_null()) {
    return;
  }

  const absl::optional<std::pair<base::FilePath, base::Version>>&
      instance_path = GetConfigPathInstance();
  if (!instance_path.has_value()) {
    // Registration not is complete yet. The policy's `on_sets_ready_` callback
    // will still be invoked once registration is done, so we don't bother to
    // save or invoke `on_sets_ready`.
    return;
  }

  if (instance_path->first.empty()) {
    // Registration is complete, but no component version exists on disk.
    CHECK(!instance_path->second.IsValid());
    std::move(on_sets_ready).Run(base::Version(), base::File());
    return;
  }

  // We use USER_BLOCKING here since First-Party Set initialization blocks
  // network navigations at startup.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), GetTaskPriority()},
      base::BindOnce(&OpenFile, instance_path->first),
      base::BindOnce(std::move(on_sets_ready), instance_path->second));
}

std::string BoolToString(bool b) {
  return b ? "true" : "false";
}

}  // namespace

namespace component_updater {

void FirstPartySetsComponentInstallerPolicy::OnRegistrationComplete() {
  if (!GetConfigPathInstance().has_value())
    GetConfigPathInstance() = std::make_pair(base::FilePath(), base::Version());
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
    const base::Value::Dict& manifest,
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
    base::Value::Dict manifest) {
  if (install_dir.empty() || GetConfigPathInstance().has_value())
    return;

  VLOG(1) << "First-Party Sets Component ready, version " << version.GetString()
          << " in " << install_dir.value();

  GetConfigPathInstance() =
      std::make_pair(GetInstalledPath(install_dir), version);

  SetFirstPartySetsConfig(std::move(on_sets_ready_));
}

// Called during startup and installation before ComponentReady().
bool FirstPartySetsComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
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
                   std::size(kFirstPartySetsPublicKeySHA256));
}

std::string FirstPartySetsComponentInstallerPolicy::GetName() const {
  return kFirstPartySetsManifestName;
}

update_client::InstallerAttributes
FirstPartySetsComponentInstallerPolicy::GetInstallerAttributes() const {
  return {
      {
          kDogfoodInstallerAttributeName,
          BoolToString(features::kFirstPartySetsIsDogfooder.Get()),
      },
  };
}

// static
void FirstPartySetsComponentInstallerPolicy::ResetForTesting() {
  GetConfigPathInstance().reset();
}

void RegisterFirstPartySetsComponent(ComponentUpdateService* cus) {
  VLOG(1) << "Registering First-Party Sets component.";

  auto policy = std::make_unique<FirstPartySetsComponentInstallerPolicy>(
      /*on_sets_ready=*/base::BindOnce([](base::Version version,
                                          base::File sets_file) {
        VLOG(1) << "Received First-Party Sets";
        content::FirstPartySetsHandler::GetInstance()->SetPublicFirstPartySets(
            version, std::move(sets_file));
      }));

  FirstPartySetsComponentInstallerPolicy* raw_policy = policy.get();
  // Dereferencing `raw_policy` this way is safe because the closure is invoked
  // by the ComponentInstaller instance, which owns `policy` (so they have the
  // same lifetime). Therefore if/when the closure is invoked, `policy` is still
  // alive.
  base::MakeRefCounted<ComponentInstaller>(std::move(policy))
      ->Register(cus,
                 base::BindOnce(
                     [](FirstPartySetsComponentInstallerPolicy* policy) {
                       policy->OnRegistrationComplete();
                     },
                     raw_policy),
                 GetTaskPriority());
}

// static
void FirstPartySetsComponentInstallerPolicy::WriteComponentForTesting(
    base::Version version,
    const base::FilePath& install_dir,
    base::StringPiece contents) {
  CHECK(base::WriteFile(GetInstalledPath(install_dir), contents));

  GetConfigPathInstance() =
      std::make_pair(GetInstalledPath(install_dir), std::move(version));
}

}  // namespace component_updater
