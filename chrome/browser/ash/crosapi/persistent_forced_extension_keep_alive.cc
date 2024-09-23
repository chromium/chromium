// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/persistent_forced_extension_keep_alive.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/hashed_extension_id.h"

namespace {

bool ShouldEnableKeepAlive(const base::Value::Dict& extension_install_list) {
  // Lacros should be kept alive if the Imprivata in-session extension is
  // installed by admin policy.
  const extensions::Feature* feature =
      extensions::FeatureProvider::GetBehaviorFeature(
          extensions::behavior_feature::kImprivataInSessionExtension);
  DCHECK(feature);

  for (auto entry : extension_install_list) {
    const extensions::ExtensionId& extension_id = entry.first;
    const extensions::HashedExtensionId& hashed_extension_id =
        extensions::HashedExtensionId(extension_id);

    if (feature->IsIdInAllowlist(hashed_extension_id))
      return true;
  }
  return false;
}

}  // namespace

namespace crosapi {

PersistentForcedExtensionKeepAlive::PersistentForcedExtensionKeepAlive(
    PrefService* user_prefs) {
  DCHECK(user_prefs);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(user_prefs);
  pref_change_registrar_->Add(
      extensions::pref_names::kInstallForceList,
      base::BindRepeating(&PersistentForcedExtensionKeepAlive::UpdateKeepAlive,
                          weak_factory_.GetWeakPtr()));

  UpdateKeepAlive();
}

PersistentForcedExtensionKeepAlive::~PersistentForcedExtensionKeepAlive() =
    default;

void PersistentForcedExtensionKeepAlive::Shutdown() {
  keep_alive_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void PersistentForcedExtensionKeepAlive::UpdateKeepAlive() {
  DCHECK(pref_change_registrar_);
  DCHECK(pref_change_registrar_->prefs());

  const base::Value::Dict& extension_install_list =
      pref_change_registrar_->prefs()->GetDict(
          extensions::pref_names::kInstallForceList);

  if (!ShouldEnableKeepAlive(extension_install_list)) {
    keep_alive_.reset();
    return;
  }
  if (!keep_alive_) {
    keep_alive_ = BrowserManager::Get()->KeepAlive(
        BrowserManager::Feature::kPersistentForcedExtension);
  }
}

// static
PersistentForcedExtensionKeepAliveFactory*
PersistentForcedExtensionKeepAliveFactory::GetInstance() {
  static base::NoDestructor<PersistentForcedExtensionKeepAliveFactory> instance;
  return instance.get();
}

PersistentForcedExtensionKeepAliveFactory::
    PersistentForcedExtensionKeepAliveFactory()
    : ProfileKeyedServiceFactory(
          "PersistentForcedExtensionKeepAlive",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

PersistentForcedExtensionKeepAliveFactory::
    ~PersistentForcedExtensionKeepAliveFactory() = default;

std::unique_ptr<KeyedService> PersistentForcedExtensionKeepAliveFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!browser_util::IsLacrosEnabled())
    return nullptr;
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return nullptr;
  if (ash::ProfileHelper::IsSigninProfile(profile)) {
    // Does not have to be registered on the sign-in profile.
    return nullptr;
  }
  return std::make_unique<PersistentForcedExtensionKeepAlive>(
      user_prefs::UserPrefs::Get(context));
}

bool PersistentForcedExtensionKeepAliveFactory::
    ServiceIsCreatedWithBrowserContext() const {
  // Service is created in the background as soon as the BrowserContext has been
  // brought up.
  return true;
}

}  // namespace crosapi
