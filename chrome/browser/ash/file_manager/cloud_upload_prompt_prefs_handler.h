// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_CLOUD_UPLOAD_PROMPT_PREFS_HANDLER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_CLOUD_UPLOAD_PROMPT_PREFS_HANDLER_H_

#include <memory>

class KeyedService;
class Profile;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace chromeos::cloud_upload {

// Registers preferences related to enterprise cloud upload flows.
void RegisterCloudUploadPromptProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Whether `profile` needs a cloud upload prompt prefs handler at all.
bool ShouldCreateCloudUploadPromptPrefsHandler(Profile* profile);

// Creates the keyed service that keeps the local and syncable cloud upload
// prompt prefs in sync for `profile`. `sync_service` may be null.
std::unique_ptr<KeyedService> CreateCloudUploadPromptPrefsHandler(
    Profile* profile,
    syncer::SyncService* sync_service);

}  // namespace chromeos::cloud_upload

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_CLOUD_UPLOAD_PROMPT_PREFS_HANDLER_H_
