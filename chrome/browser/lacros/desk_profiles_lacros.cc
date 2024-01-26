// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/desk_profiles_lacros.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/lacros/profile_util.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace crosapi {
namespace {

mojom::LacrosProfileSummaryPtr CreateProfileSummary(
    const ProfileAttributesEntry& entry) {
  auto summary = mojom::LacrosProfileSummary::New();

  summary->profile_id = HashProfilePathToProfileId(entry.GetPath());
  summary->name = base::UTF16ToUTF8(entry.GetName());
  summary->email = base::UTF16ToUTF8(entry.GetUserName());
  summary->icon = *entry.GetAvatarIcon().ToImageSkia();

  return summary;
}

}  // namespace

DeskProfilesLacros::DeskProfilesLacros(ProfileManager* profile_manager,
                                       mojom::DeskProfileObserver* remote)
    : profile_manager_(profile_manager), remote_(remote) {
  storage_observer_.Observe(&profile_manager_->GetProfileAttributesStorage());
  manager_observer_.Observe(profile_manager_);

  std::vector<ProfileAttributesEntry*> entries =
      profile_manager_->GetProfileAttributesStorage()
          .GetAllProfilesAttributesSortedByNameWithCheck();

  std::vector<mojom::LacrosProfileSummaryPtr> profiles;
  profiles.reserve(entries.size());

  for (const auto* entry : entries) {
    profiles.push_back(CreateProfileSummary(*entry));
  }

  remote_->OnProfileUpsert(std::move(profiles));
}

DeskProfilesLacros::~DeskProfilesLacros() = default;

void DeskProfilesLacros::OnProfileAdded(const base::FilePath& profile_path) {
  SendProfileUpsert(profile_path);
}

void DeskProfilesLacros::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  remote_->OnProfileRemoved(HashProfilePathToProfileId(profile_path));
}

void DeskProfilesLacros::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  SendProfileUpsert(profile_path);
}

void DeskProfilesLacros::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  SendProfileUpsert(profile_path);
}

void DeskProfilesLacros::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  SendProfileUpsert(profile_path);
}

void DeskProfilesLacros::OnProfileManagerDestroying() {
  // To avoid danging pointer errors on shutdown.
  profile_manager_ = nullptr;
  manager_observer_.Reset();
  storage_observer_.Reset();
}

void DeskProfilesLacros::OnProfileAdded(Profile* profile) {
  // We are not actually using this overload. However, it must be defined since
  // we *are* using `ProfileAttributesStorageObserver::OnProfileAdded` and C++
  // won't let us get away with just defining one the competing overloads.
}

void DeskProfilesLacros::SendProfileUpsert(const base::FilePath& profile_path) {
  CHECK(profile_manager_);

  if (auto* entry = profile_manager_->GetProfileAttributesStorage()
                        .GetProfileAttributesWithPath(profile_path)) {
    std::vector<mojom::LacrosProfileSummaryPtr> profiles;
    profiles.push_back(CreateProfileSummary(*entry));
    remote_->OnProfileUpsert(std::move(profiles));
  }
}

}  // namespace crosapi
