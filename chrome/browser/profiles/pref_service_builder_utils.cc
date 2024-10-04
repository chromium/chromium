// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/pref_service_builder_utils.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/profile_pref_store_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_value_store.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/network_service_instance.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Text content of README file created in each profile directory. Both %s
// placeholders must contain the product name. This is not localizable and hence
// not in resources.
const char kReadmeText[] =
    "%s settings and storage represent user-selected preferences and "
    "information and MUST not be extracted, overwritten or modified except "
    "through %s defined APIs.";

}  // namespace

void CreateProfileReadme(const base::FilePath& profile_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath readme_path = profile_path.Append(chrome::kReadmeFilename);
  std::string product_name = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  std::string readme_text = base::StringPrintf(
      kReadmeText, product_name.c_str(), product_name.c_str());
  if (!base::WriteFile(readme_path, readme_text)) {
    LOG(ERROR) << "Could not create README file.";
  }
}

void RegisterProfilePrefs(bool is_signin_profile,
                          const std::string& locale,
                          user_prefs::PrefRegistrySyncable* pref_registry) {
#if BUILDFLAG(IS_CHROMEOS)
  if (is_signin_profile)
    RegisterSigninProfilePrefs(pref_registry, GetCountry());
  else
#endif
    RegisterUserProfilePrefs(pref_registry, locale);

  SimpleDependencyManager::GetInstance()->RegisterProfilePrefsForServices(
      pref_registry);
  BrowserContextDependencyManager::GetInstance()
      ->RegisterProfilePrefsForServices(pref_registry);
}

std::unique_ptr<sync_preferences::PrefServiceSyncable> CreateProfilePrefService(
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
    PrefStore* extension_pref_store,
    policy::PolicyService* policy_service,
    policy::ChromeBrowserPolicyConnector* browser_policy_connector,
    mojo::PendingRemote<prefs::mojom::TrackedPreferenceValidationDelegate>
        pref_validation_delegate,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    SimpleFactoryKey* key,
    const base::FilePath& profile_path,
    bool async_prefs) {
  supervised_user::SupervisedUserSettingsService* supervised_user_settings =
      SupervisedUserSettingsServiceFactory::GetForKey(key);
  supervised_user_settings->Init(profile_path, io_task_runner, !async_prefs);
  return chrome_prefs::CreateProfilePrefs(
      profile_path, std::move(pref_validation_delegate), policy_service,
      supervised_user_settings, extension_pref_store, pref_registry,
      browser_policy_connector, async_prefs, io_task_runner);
}
