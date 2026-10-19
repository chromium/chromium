// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_key.h"

#include "base/check.h"
#include "build/build_config.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile_key_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

ProfileKey::ProfileKey(const base::FilePath& path, ProfileKey* original_key)
    : SimpleFactoryKey(path, original_key != nullptr /* is_off_the_record */),
      original_key_(original_key) {}

ProfileKey::~ProfileKey() = default;

ProfileKey* ProfileKey::GetOriginalKey() {
  if (original_key_)
    return original_key_;
  return this;
}

PrefService* ProfileKey::GetPrefs() {
  CHECK(prefs_, base::NotFatalUntil::M162);
  return prefs_;
}

void ProfileKey::SetPrefs(PrefService* prefs) {
  CHECK(!prefs_, base::NotFatalUntil::M162);
  prefs_ = prefs;
}

leveldb_proto::ProtoDatabaseProvider* ProfileKey::GetProtoDatabaseProvider() {
  CHECK(db_provider_, base::NotFatalUntil::M162);
  return db_provider_;
}

void ProfileKey::SetProtoDatabaseProvider(
    leveldb_proto::ProtoDatabaseProvider* db_provider) {
  // If started from reduced mode on Android, the db provider is set by
  // both StartupData and ProfileImpl.
  CHECK(!db_provider_ || db_provider_ == db_provider,
        base::NotFatalUntil::M162);
  db_provider_ = db_provider;
}

// static
ProfileKey* ProfileKey::FromSimpleFactoryKey(SimpleFactoryKey* key) {
  return key ? static_cast<ProfileKey*>(key) : nullptr;
}

#if BUILDFLAG(IS_ANDROID)
ProfileKeyAndroid* ProfileKey::GetProfileKeyAndroid() {
  if (!profile_key_android_)
    profile_key_android_ = std::make_unique<ProfileKeyAndroid>(this);
  return profile_key_android_.get();
}
#endif  // BUILDFLAG(IS_ANDROID)
