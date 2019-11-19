// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STARTUP_DATA_H_
#define CHROME_BROWSER_STARTUP_DATA_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {
class ProfilePolicyConnector;
class SchemaRegistryService;
class UserCloudPolicyManager;
}  // namespace policy

namespace sync_preferences {
class PrefServiceSyncable;
}

class PrefService;
class ProfileKey;
class ChromeFeatureListCreator;

// The StartupData owns any pre-created objects in //chrome before the full
// browser starts, including the ChromeFeatureListCreator and the Profile's
// PrefService. See doc:
// https://docs.google.com/document/d/1ybmGWRWXu0aYNxA99IcHFesDAslIaO1KFP6eGdHTJaE/edit#heading=h.7bk05syrcom
class StartupData {
 public:
  StartupData();
  ~StartupData();

  // Records core profile settings into the SystemProfileProto. It is important
  // when Chrome is running in the reduced mode, which doesn't start UMA
  // recording but persists all of the UMA data into a memory mapped file. The
  // file will be picked up by Chrome next time it is launched in the full
  // browser mode.
  void RecordCoreSystemProfile();

#if defined(OS_ANDROID)
  // Initializes all necessary parameters to create the Profile's PrefService.
  void CreateProfilePrefService();

  // Returns whether a PrefService has been created.
  bool HasBuiltProfilePrefService();

  ProfileKey* GetProfileKey();

  // Passes ownership of the |key_| to the caller.
  std::unique_ptr<ProfileKey> TakeProfileKey();

  // Passes ownership of the |schema_registry_service_| to the caller.
  std::unique_ptr<policy::SchemaRegistryService> TakeSchemaRegistryService();

  // Passes ownership of the |user_cloud_policy_manager_| to the caller.
  std::unique_ptr<policy::UserCloudPolicyManager> TakeUserCloudPolicyManager();

  // Passes ownership of the |profile_policy_connector_| to the caller.
  std::unique_ptr<policy::ProfilePolicyConnector> TakeProfilePolicyConnector();

  // Passes ownership of the |pref_registry_| to the caller.
  scoped_refptr<user_prefs::PrefRegistrySyncable> TakePrefRegistrySyncable();

  // Passes ownership of the |prefs_| to the caller.
  std::unique_ptr<sync_preferences::PrefServiceSyncable>
  TakeProfilePrefService();

  // Passes ownership of the |proto_db_provider_| to the caller.
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider>
  TakeProtoDatabaseProvider();
#endif

  ChromeFeatureListCreator* chrome_feature_list_creator() {
    return chrome_feature_list_creator_.get();
  }

 private:
#if defined(OS_ANDROID)
  void PreProfilePrefServiceInit();
  void CreateServicesInternal();

  std::unique_ptr<ProfileKey> key_;

  std::unique_ptr<policy::SchemaRegistryService> schema_registry_service_;
  std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
  std::unique_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;

  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;

  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> proto_db_provider_;
#endif

  std::unique_ptr<ChromeFeatureListCreator> chrome_feature_list_creator_;

  DISALLOW_COPY_AND_ASSIGN(StartupData);
};

#endif  // CHROME_BROWSER_STARTUP_DATA_H_
