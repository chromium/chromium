// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/managed_profile_creator.h"

#include <string>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/identifiers/profile_id_delegate_impl.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"

ManagedProfileCreator::ManagedProfileCreator(
    Profile* source_profile,
    const std::string& id,
    const std::u16string& local_profile_name,
    std::unique_ptr<ManagedProfileCreationDelegate> delegate,
    base::OnceCallback<void(base::WeakPtr<Profile>)> callback,
    std::string preset_guid)
    : source_profile_(source_profile),
      id_(id),
      delegate_(std::move(delegate)),
      expected_profile_path_(g_browser_process->profile_manager()
                                 ->GetNextExpectedProfileDirectoryPath()),
      callback_(std::move(callback)),
      preset_guid_(preset_guid) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager_observer_.Observe(profile_manager);

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  profile_observation_.Observe(&storage);

  auto icon_index = storage.ChooseAvatarIconIndexForNewProfile();
  std::u16string name = local_profile_name.empty()
                            ? storage.ChooseNameForNewProfile(icon_index)
                            : local_profile_name;
  ProfileManager::CreateMultiProfileAsync(
      name, icon_index, /*is_hidden=*/id_.empty(),
      base::BindRepeating(&ManagedProfileCreator::OnNewProfileInitialized,
                          weak_pointer_factory_.GetWeakPtr()),
      base::BindOnce(&ManagedProfileCreator::OnNewProfileCreated,
                     weak_pointer_factory_.GetWeakPtr()));
}

ManagedProfileCreator::ManagedProfileCreator(
    Profile* source_profile,
    const base::FilePath& target_profile_path,
    std::unique_ptr<ManagedProfileCreationDelegate> delegate,
    base::OnceCallback<void(base::WeakPtr<Profile>)> callback)
    : source_profile_(source_profile),
      delegate_(std::move(delegate)),
      callback_(std::move(callback)) {
  // Make sure the callback is not called synchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&ProfileManager::LoadProfileByPath),
          base::Unretained(g_browser_process->profile_manager()),
          target_profile_path, /*incognito=*/false,
          base::BindOnce(&ManagedProfileCreator::OnNewProfileInitialized,
                         weak_pointer_factory_.GetWeakPtr())));
}


ManagedProfileCreator::~ManagedProfileCreator() = default;

void ManagedProfileCreator::OnProfileAdded(
    const base::FilePath& profile_path) {
  if (expected_profile_path_ != profile_path) {
    return;
  }
  auto* entry = g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile_path);
  CHECK(entry);
  if (!id_.empty()) {
    entry->SetProfileManagementId(id_);
  }
  delegate_->SetManagedAttributesForProfile(entry);
}

void ManagedProfileCreator::OnProfileCreationStarted(Profile* profile) {
  if (expected_profile_path_ != profile->GetPath() || preset_guid_.empty()) {
    return;
  }

  enterprise::PresetProfileManagmentData::Get(profile)->SetGuid(preset_guid_);
  profile_manager_observer_.Reset();
}

void ManagedProfileCreator::OnNewProfileCreated(Profile* new_profile) {
  if (!new_profile || expected_profile_path_ != new_profile->GetPath()) {
    return;
  }
  delegate_->CheckManagedProfileStatus(new_profile);
}

void ManagedProfileCreator::OnNewProfileInitialized(Profile* new_profile) {
  delegate_->OnManagedProfileInitialized(
    source_profile_,
    new_profile,
    std::move(callback_)
  );
}
