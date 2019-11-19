// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PREF_SERVICE_BUILDER_UTILS_H_
#define CHROME_BROWSER_PROFILES_PREF_SERVICE_BUILDER_UTILS_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

class PrefStore;
class SimpleFactoryKey;

namespace base {
class SequencedTaskRunner;
}

namespace policy {
class PolicyService;
class ChromeBrowserPolicyConnector;
}  // namespace policy

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This file includes multiple helper functions to create the Profile's
// PrefService. Note: please update all of the callers if updating any helper
// function. Currently, these code are called in both ProfileImpl and
// StartupData.

void CreateProfileReadme(const base::FilePath& profile_path);

// Called to register all of the prefs before creating the PrefService.
void RegisterProfilePrefs(bool is_signin_profile,
                          const std::string& locale,
                          user_prefs::PrefRegistrySyncable* pref_registry);

// Creates the PrefService.
std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService(
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
    PrefStore* extension_pref_store,
    policy::PolicyService* policy_service,
    policy::ChromeBrowserPolicyConnector* browser_policy_connector,
    mojo::PendingRemote<prefs::mojom::TrackedPreferenceValidationDelegate>
        pref_validation_delegate,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    SimpleFactoryKey* key,
    const base::FilePath& path,
    bool async_prefs);

#endif  // CHROME_BROWSER_PROFILES_PREF_SERVICE_BUILDER_UTILS_H_
