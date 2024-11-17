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
#include "base/task/bind_post_task.h"
#include "build/build_config.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/credentials_cleaner_runner_factory.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/password_manager/password_store_backend_factory.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

using password_manager::AffiliatedMatchHelper;
using password_manager::PasswordForm;
using password_manager::PasswordStore;
using password_manager::PasswordStoreInterface;
using password_manager::UnsyncedCredentialsDeletionNotifier;

#if !BUILDFLAG(IS_ANDROID)
// Returns a repeating callback that to show warning UI that credentials are
// about to be deleted. Note that showing the UI is asynchronous, but safe to
// call from any sequence.
UnsyncedCredentialsDeletionNotifier CreateUnsyncedCredentialsDeletionNotifier(
    Profile& profile) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Tries to show warning UI that `credentials` will be deleted.
  auto try_to_show_ui = base::BindRepeating(
      [](base::WeakPtr<Profile> profile,
         std::vector<PasswordForm> credentials) {
        if (!profile) {
          return;
        }
        Browser* browser = chrome::FindBrowserWithProfile(profile.get());
        if (!browser) {
          return;
        }
        content::WebContents* web_contents =
            browser->tab_strip_model()->GetActiveWebContents();
        if (!web_contents) {
          return;
        }
        if (auto* ui_controller =
                ManagePasswordsUIController::FromWebContents(web_contents)) {
          ui_controller->NotifyUnsyncedCredentialsWillBeDeleted(
              std::move(credentials));
        }
      },
      profile.GetWeakPtr());
  return base::BindPostTask(content::GetUIThreadTaskRunner({}),
                            std::move(try_to_show_ui));
}
#endif  // !BUILDFLAG(IS_ANDROID)

scoped_refptr<RefcountedKeyedService> BuildPasswordStore(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  if (!password_manager_android_util::IsInternalBackendPresent()) {
    LOG(ERROR)
        << "Password store is not supported: use_login_database_as_backend is "
           "false when Chrome's internal backend is not present. Please, set "
           "use_login_database_as_backend=true in the args.gn file to enable "
           "Chrome password store.";
    return nullptr;
  }
#endif

  Profile* profile = Profile::FromBrowserContext(context);

  CHECK(password_manager::features_util::CanCreateAccountStore(
      profile->GetPrefs()));
  DCHECK(!profile->IsOffTheRecord());

  os_crypt_async::OSCryptAsync* os_crypt_async =
      base::FeatureList::IsEnabled(
          password_manager::features::kUseAsyncOsCryptInLoginDatabase)
          ? g_browser_process->os_crypt_async()
          : nullptr;

  scoped_refptr<password_manager::PasswordStore> ps =
#if BUILDFLAG(IS_ANDROID)
      new password_manager::PasswordStore(CreateAccountPasswordStoreBackend(
          profile->GetPath(), profile->GetPrefs(),
          /*unsynced_deletions_notifier=*/base::NullCallback(),
          os_crypt_async));
#else
      new password_manager::PasswordStore(CreateAccountPasswordStoreBackend(
          profile->GetPath(), profile->GetPrefs(),
          CreateUnsyncedCredentialsDeletionNotifier(*profile), os_crypt_async));
#endif

  affiliations::AffiliationService* affiliation_service =
      AffiliationServiceFactory::GetForProfile(profile);
  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper =
      std::make_unique<AffiliatedMatchHelper>(affiliation_service);

  ps->Init(profile->GetPrefs(), std::move(affiliated_match_helper));

  auto network_context_getter = base::BindRepeating(
      [](Profile* profile) -> network::mojom::NetworkContext* {
        if (!g_browser_process->profile_manager()->IsValidProfile(profile)) {
          return nullptr;
        }
        return profile->GetDefaultStoragePartition()->GetNetworkContext();
      },
      profile);
  password_manager::SanitizeAndMigrateCredentials(
      CredentialsCleanerRunnerFactory::GetForProfile(profile), ps,
      password_manager::kAccountStore, profile->GetPrefs(), base::Seconds(60),
      network_context_getter);

#if !BUILDFLAG(IS_ANDROID)
  // Android gets logins with affiliations directly from the backend.
  std::unique_ptr<password_manager::PasswordAffiliationSourceAdapter>
      password_affiliation_adapter = std::make_unique<
          password_manager::PasswordAffiliationSourceAdapter>();
  password_affiliation_adapter->RegisterPasswordStore(ps.get());
  affiliation_service->RegisterSource(std::move(password_affiliation_adapter));
#endif

  return ps;
}

}  // namespace

// static
scoped_refptr<PasswordStoreInterface>
AccountPasswordStoreFactory::GetForProfile(Profile* profile,
                                           ServiceAccessType access_type) {
  if (!password_manager::features_util::CanCreateAccountStore(
          profile->GetPrefs())) {
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
bool AccountPasswordStoreFactory::HasStore(Profile* profile) {
  return GetInstance()->GetServiceForBrowserContext(
             profile, /*create=*/false) != nullptr;
}

// static
AccountPasswordStoreFactory* AccountPasswordStoreFactory::GetInstance() {
  static base::NoDestructor<AccountPasswordStoreFactory> instance;
  return instance.get();
}

AccountPasswordStoreFactory::AccountPasswordStoreFactory()
    : RefcountedProfileKeyedServiceFactory(
          "AccountPasswordStore",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(CredentialsCleanerRunnerFactory::GetInstance());
}

AccountPasswordStoreFactory::~AccountPasswordStoreFactory() = default;

AccountPasswordStoreFactory::TestingFactory
AccountPasswordStoreFactory::GetDefaultFactoryForTesting() {
  return base::BindRepeating(&BuildPasswordStore);
}

scoped_refptr<RefcountedKeyedService>
AccountPasswordStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildPasswordStore(context);
}

bool AccountPasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
