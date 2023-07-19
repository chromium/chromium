// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/token_managed_profile_creator.h"

#include <string>

#include "base/check.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"

TokenManagedProfileCreator::TokenManagedProfileCreator(
    Profile* source_profile,
    const std::string& id,
    const std::string& enrollment_token,
    const std::u16string& local_profile_name,
    base::OnceCallback<void(base::WeakPtr<Profile>)> callback)
    : source_profile_(source_profile),
      id_(id),
      enrollment_token_(enrollment_token),
      expected_profile_path_(g_browser_process->profile_manager()
                                 ->GetNextExpectedProfileDirectoryPath()),
      callback_(std::move(callback)) {
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  profile_observation_.Observe(&storage);
  auto icon_index = storage.ChooseAvatarIconIndexForNewProfile();
  std::u16string name = local_profile_name.empty()
                            ? storage.ChooseNameForNewProfile(icon_index)
                            : local_profile_name;
  ProfileManager::CreateMultiProfileAsync(
      name, icon_index, /*is_hidden=*/id_.empty(),
      base::BindRepeating(&TokenManagedProfileCreator::OnNewProfileInitialized,
                          weak_pointer_factory_.GetWeakPtr()),
      base::BindOnce(&TokenManagedProfileCreator::OnNewProfileCreated,
                     weak_pointer_factory_.GetWeakPtr()));
}

TokenManagedProfileCreator::TokenManagedProfileCreator(
    Profile* source_profile,
    const base::FilePath& target_profile_path,
    base::OnceCallback<void(base::WeakPtr<Profile>)> callback)
    : source_profile_(source_profile), callback_(std::move(callback)) {
  // Make sure the callback is not called synchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&ProfileManager::LoadProfileByPath),
          base::Unretained(g_browser_process->profile_manager()),
          target_profile_path, /*incognito=*/false,
          base::BindOnce(&TokenManagedProfileCreator::OnNewProfileInitialized,
                         weak_pointer_factory_.GetWeakPtr())));
}

TokenManagedProfileCreator::~TokenManagedProfileCreator() = default;

void TokenManagedProfileCreator::OnProfileAdded(
    const base::FilePath& profile_path) {
  if (expected_profile_path_ != profile_path) {
    return;
  }

  auto* entry = g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile_path);
  DCHECK(entry);
  if (!id_.empty()) {
    entry->SetProfileManagementId(id_);
  }
  if (!enrollment_token_.empty()) {
    entry->SetProfileManagementEnrollmentToken(enrollment_token_);
  }
}

void TokenManagedProfileCreator::OnNewProfileCreated(Profile* new_profile) {
  if (!new_profile || expected_profile_path_ != new_profile->GetPath()) {
    return;
  }
  DCHECK(!new_profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
}

void TokenManagedProfileCreator::OnNewProfileInitialized(Profile* new_profile) {
  // base::Unretained is fine because `cookies_mover_` is owned by this.
  cookies_mover_ = std::make_unique<signin_util::CookiesMover>(
      source_profile_->GetWeakPtr(), new_profile->GetWeakPtr(),
      base::BindOnce(std::move(callback_), new_profile->GetWeakPtr()));
  cookies_mover_->StartMovingCookies();
}
