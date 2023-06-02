// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/account_password_store_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/affiliations_prefetcher_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/credentials_cleaner_runner_factory.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/password_manager/core/browser/password_store_built_in_backend.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using password_manager::AffiliatedMatchHelper;
using password_manager::PasswordStore;
using password_manager::PasswordStoreInterface;
using password_manager::UnsyncedCredentialsDeletionNotifier;

namespace {

#if !BUILDFLAG(IS_ANDROID)
class UnsyncedCredentialsDeletionNotifierImpl
    : public UnsyncedCredentialsDeletionNotifier {
 public:
  explicit UnsyncedCredentialsDeletionNotifierImpl(Profile* profile);
  ~UnsyncedCredentialsDeletionNotifierImpl() override = default;

  // Finds the last active tab and notifies their ManagePasswordsUIController.
  void Notify(std::vector<password_manager::PasswordForm> credentials) override;
  base::WeakPtr<UnsyncedCredentialsDeletionNotifier> GetWeakPtr() override;

 private:
  const raw_ptr<Profile, DanglingUntriaged> profile_;
  base::WeakPtrFactory<UnsyncedCredentialsDeletionNotifier> weak_ptr_factory_{
      this};
};

UnsyncedCredentialsDeletionNotifierImpl::
    UnsyncedCredentialsDeletionNotifierImpl(Profile* profile)
    : profile_(profile) {}

void UnsyncedCredentialsDeletionNotifierImpl::Notify(
    std::vector<password_manager::PasswordForm> credentials) {
  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser)
    return;
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  auto* ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents);
  if (!ui_controller)
    return;
  ui_controller->NotifyUnsyncedCredentialsWillBeDeleted(std::move(credentials));
}

base::WeakPtr<UnsyncedCredentialsDeletionNotifier>
UnsyncedCredentialsDeletionNotifierImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

// static
scoped_refptr<PasswordStoreInterface>
AccountPasswordStoreFactory::GetForProfile(Profile* profile,
                                           ServiceAccessType access_type) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return nullptr;
  }
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      profile->IsOffTheRecord()) {
    return nullptr;
  }
  return base::WrapRefCounted(
      static_cast<password_manager::PasswordStoreInterface*>(
          GetInstance()->GetServiceForBrowserContext(profile, true).get()));
}

// static
AccountPasswordStoreFactory* AccountPasswordStoreFactory::GetInstance() {
  static base::NoDestructor<AccountPasswordStoreFactory> instance;
  return instance.get();
}

AccountPasswordStoreFactory::AccountPasswordStoreFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "AccountPasswordStore",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CredentialsCleanerRunnerFactory::GetInstance());
}

AccountPasswordStoreFactory::~AccountPasswordStoreFactory() = default;

scoped_refptr<RefcountedKeyedService>
AccountPasswordStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

  Profile* profile = Profile::FromBrowserContext(context);

  DCHECK(!profile->IsOffTheRecord());

  // Incognito profiles don't have their own password stores. Guest, or system
  // profiles aren't relevant for Password Manager, and no PasswordStore should
  // even be created for those types of profiles.
  if (!profile->IsRegularProfile())
    return nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Ash, there are additional non-interesting profile types (sign-in
  // profile and lockscreen profile).
  if (!ash::ProfileHelper::IsUserProfile(profile))
    return nullptr;
#endif

  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForAccountStorage(
          profile->GetPath()));

  scoped_refptr<password_manager::PasswordStore> ps =
#if BUILDFLAG(IS_ANDROID)
      new password_manager::PasswordStore(
          std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
              std::move(login_db)));
#else
      new password_manager::PasswordStore(
          std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
              std::move(login_db),
              std::make_unique<UnsyncedCredentialsDeletionNotifierImpl>(
                  profile)));
#endif

  password_manager::AffiliationService* affiliation_service =
      AffiliationServiceFactory::GetForProfile(profile);
  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper =
      std::make_unique<AffiliatedMatchHelper>(affiliation_service);

  ps->Init(profile->GetPrefs(), std::move(affiliated_match_helper));

  auto network_context_getter = base::BindRepeating(
      [](Profile* profile) -> network::mojom::NetworkContext* {
        if (!g_browser_process->profile_manager()->IsValidProfile(profile))
          return nullptr;
        return profile->GetDefaultStoragePartition()->GetNetworkContext();
      },
      profile);
  password_manager_util::RemoveUselessCredentials(
      CredentialsCleanerRunnerFactory::GetForProfile(profile), ps,
      profile->GetPrefs(), base::Seconds(60), network_context_getter);

  AffiliationsPrefetcherFactory::GetForProfile(profile)->RegisterPasswordStore(
      ps.get());

  return ps;
}

content::BrowserContext* AccountPasswordStoreFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool AccountPasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
