// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/games_component_installer.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "components/games/core/games_features.h"
#include "components/games/core/games_prefs.h"
#include "components/games/core/games_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

using content::BrowserThread;

namespace component_updater {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: nmjnfgmoafbajdnpaondodmmmhopidgn
const uint8_t kGamesPublicKeySHA256[32] = {
    0xdc, 0x9d, 0x56, 0xce, 0x05, 0x10, 0x93, 0xdf, 0x0e, 0xd3, 0xe3,
    0xcc, 0xc7, 0xef, 0x83, 0x6d, 0xd5, 0x43, 0x4b, 0x5b, 0x1a, 0xeb,
    0x0d, 0xf0, 0xaf, 0x90, 0xd6, 0xdd, 0x4c, 0x0b, 0xb9, 0x3f};
static_assert(base::size(kGamesPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

const char kGamesManifestName[] = "Game Data Files";

void UpdateInstallDirPref(PrefService* prefs,
                          const base::FilePath& install_dir) {
  games::prefs::SetInstallDirPath(prefs, install_dir);
  VLOG(1) << "Updated Games data files pref.";
}

void RegisterGamesComponentHelper(ComponentUpdateService* cus,
                                  PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO crbug.com/1020159 Create a new Enterprise policy.
  if (base::FeatureList::IsEnabled(games::features::kGamesHub)) {
    VLOG(1) << "Registering Games component.";

    auto lambda = [](PrefService* prefs, const base::FilePath& install_dir) {
      content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&UpdateInstallDirPref, prefs, install_dir));
    };

    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<GamesComponentInstallerPolicy>(
            base::BindRepeating(lambda, prefs)));
    installer->Register(cus, base::OnceClosure());
  }
}

}  // namespace

GamesComponentInstallerPolicy::GamesComponentInstallerPolicy(
    OnGamesComponentReadyCallback callback)
    : on_component_ready_callback_(callback) {}

GamesComponentInstallerPolicy::~GamesComponentInstallerPolicy() = default;

bool GamesComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // Verify that the Games Catalog file exists; that is the most important file
  // of the component and it should always be there.
  return base::PathExists(games::GetGamesCatalogPath(install_dir));
}

bool GamesComponentInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return false;
}

bool GamesComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
GamesComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void GamesComponentInstallerPolicy::OnCustomUninstall() {}

void GamesComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  on_component_ready_callback_.Run(install_dir);
}

base::FilePath GamesComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("Games"));
}

void GamesComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kGamesPublicKeySHA256,
               kGamesPublicKeySHA256 + base::size(kGamesPublicKeySHA256));
}

std::string GamesComponentInstallerPolicy::GetName() const {
  return kGamesManifestName;
}

update_client::InstallerAttributes
GamesComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterGamesComponent(ComponentUpdateService* cus, PrefService* prefs) {
  // We delay the registration because we are not required in the critical path
  // during browser setup.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&RegisterGamesComponentHelper, cus, prefs));
}

}  // namespace component_updater
