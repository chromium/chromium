// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_sync_observer.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

// A dictionary containing info about user image.
const char kUserImageInfo[] = "user_image_info";
// Path to value with image index.
const char kImageIndex[] = "image_index";

bool IsIndexSupported(int index) {
  return default_user_image::IsValidIndex(index) ||
         (index == user_manager::UserImage::Type::kProfile);
}

}  // anonymous namespace

UserImageSyncObserver::UserImageSyncObserver(const user_manager::User* user)
    : user_(user),
      prefs_(nullptr),
      is_synced_(false),
      local_image_changed_(false) {
  user_manager::UserManager::Get()->AddObserver(this);

  if (Profile* profile = ProfileHelper::Get()->GetProfileByUser(user)) {
    OnProfileGained(profile);
  } else {
    auto* session_manager = session_manager::SessionManager::Get();
    // SessionManager might not exist in unit tests.
    if (session_manager) {
      session_observation_.Observe(session_manager);
    }
  }
}

UserImageSyncObserver::~UserImageSyncObserver() {
  if (!is_synced_ && prefs_) {
    prefs_->RemoveObserver(this);
  }
  if (pref_change_registrar_) {
    pref_change_registrar_->RemoveAll();
  }

  user_manager::UserManager::Get()->RemoveObserver(this);
}

// static
void UserImageSyncObserver::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry_) {
  registry_->RegisterDictionaryPref(
      kUserImageInfo,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
}

void UserImageSyncObserver::OnProfileGained(Profile* profile) {
  prefs_ = PrefServiceSyncableFromProfile(profile);
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs_);
  pref_change_registrar_->Add(
      kUserImageInfo,
      base::BindRepeating(&UserImageSyncObserver::OnPreferenceChanged,
                          base::Unretained(this)));
  is_synced_ = prefs_->AreOsPriorityPrefsSyncing();
  if (!is_synced_) {
    prefs_->AddObserver(this);
  } else {
    OnInitialSync();
  }
}

void UserImageSyncObserver::OnInitialSync() {
  int synced_index;
  if (!GetSyncedImageIndex(&synced_index) || local_image_changed_) {
    UpdateSyncedImageFromLocal();
  } else if (IsIndexSupported(synced_index)) {
    UpdateLocalImageFromSynced();
  }
}

void UserImageSyncObserver::OnPreferenceChanged(const std::string& pref_name) {
  // OnPreferenceChanged can be called before OnIsSyncingChanged.
  if (!is_synced_) {
    is_synced_ = true;
    prefs_->RemoveObserver(this);
    OnInitialSync();
  } else {
    UpdateLocalImageFromSynced();
  }
}

void UserImageSyncObserver::OnUserProfileLoaded(const AccountId& account_id) {
  if (user_->GetAccountId() != account_id) {
    return;
  }

  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  DCHECK(profile);
  session_observation_.Reset();
  OnProfileGained(profile);
}

void UserImageSyncObserver::OnUserImageChanged(const user_manager::User& user) {
  if (is_synced_) {
    UpdateSyncedImageFromLocal();
  } else {
    local_image_changed_ = true;
  }
}

void UserImageSyncObserver::OnIsSyncingChanged() {
  is_synced_ = prefs_->AreOsPriorityPrefsSyncing();
  if (is_synced_) {
    prefs_->RemoveObserver(this);
    OnInitialSync();
  }
}

void UserImageSyncObserver::UpdateSyncedImageFromLocal() {
  int local_index = user_->image_index();
  if (!IsIndexSupported(local_index)) {
    local_index = user_manager::UserImage::Type::kInvalid;
  }
  int synced_index;
  if (GetSyncedImageIndex(&synced_index) && (synced_index == local_index)) {
    return;
  }
  ScopedDictPrefUpdate update(prefs_, kUserImageInfo);
  base::Value::Dict& dict = update.Get();
  dict.Set(kImageIndex, local_index);
  VLOG(1) << "Saved avatar index " << local_index << " to sync.";
}

void UserImageSyncObserver::UpdateLocalImageFromSynced() {
  int synced_index;
  GetSyncedImageIndex(&synced_index);
  int local_index = user_->image_index();
  if ((synced_index == local_index) || !IsIndexSupported(synced_index)) {
    return;
  }
  UserImageManagerImpl* image_manager =
      UserImageManagerRegistry::Get()->GetManager(user_->GetAccountId());
  if (synced_index == user_manager::UserImage::Type::kProfile) {
    image_manager->SaveUserImageFromProfileImage();
  } else {
    image_manager->SaveUserDefaultImageIndex(synced_index);
  }
  VLOG(1) << "Loaded avatar index " << synced_index << " from sync.";
}

bool UserImageSyncObserver::GetSyncedImageIndex(int* index) {
  *index = user_manager::UserImage::Type::kInvalid;
  const base::Value::Dict& dict = prefs_->GetDict(kUserImageInfo);
  std::optional<int> maybe_index = dict.FindInt(kImageIndex);
  if (!maybe_index.has_value()) {
    *index = user_manager::UserImage::Type::kInvalid;
    return false;
  }

  *index = maybe_index.value();
  return true;
}

}  // namespace ash
