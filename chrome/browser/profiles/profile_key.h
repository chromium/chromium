// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEY_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

#if defined(OS_ANDROID)
#include "chrome/browser/profiles/profile_key_android.h"
#endif  // OS_ANDROID

class PrefService;

// An embryonic Profile with only fields accessible in reduced mode.
// Used as a SimpleFactoryKey.
class ProfileKey : public SimpleFactoryKey {
 public:
  ProfileKey(const base::FilePath& path,
             ProfileKey* original_key = nullptr);
  ~ProfileKey() override;

  // Profile-specific APIs needed in reduced mode:
  ProfileKey* GetOriginalKey();
  PrefService* GetPrefs();
  void SetPrefs(PrefService* prefs);

  // Gets a pointer to a ProtoDatabaseProvider, this instance is owned by
  // StartupData in Android's reduced mode, and by StoragePartition in all other
  // cases. Virtual for testing.
  virtual leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProvider();
  void SetProtoDatabaseProvider(
      leveldb_proto::ProtoDatabaseProvider* db_provider);

  static ProfileKey* FromSimpleFactoryKey(SimpleFactoryKey* key);

#if defined(OS_ANDROID)
  ProfileKeyAndroid* GetProfileKeyAndroid();
#endif  // OS_ANDROID

 private:
  PrefService* prefs_ = nullptr;
  leveldb_proto::ProtoDatabaseProvider* db_provider_ = nullptr;

  // Points to the original (non off-the-record) ProfileKey.
  ProfileKey* original_key_ = nullptr;

#if defined(OS_ANDROID)
  std::unique_ptr<ProfileKeyAndroid> profile_key_android_;
#endif  // OS_ANDROID

  DISALLOW_COPY_AND_ASSIGN(ProfileKey);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEY_H_
