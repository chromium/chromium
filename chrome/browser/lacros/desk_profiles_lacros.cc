// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/desk_profiles_lacros.h"

#include "base/containers/span.h"
#include "base/hash/legacy_hash.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace crosapi {
namespace {

// Computes a stable profile identifier based on the profile path. `CityHash64`
// is defined as unchanging.
uint64_t ProfileHash(const std::string& profile_path) {
  // Leave the lowest bit unset to account for potential future updates.
  return base::legacy::CityHash64(base::as_bytes(base::make_span(profile_path)))
         << 1;
}

mojom::LacrosProfileSummaryPtr CreateProfileSummary(
    const ProfileAttributesEntry& entry) {
  auto summary = mojom::LacrosProfileSummary::New();

  summary->profile_id = ProfileHash(entry.GetPath().value());
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
  const std::string& path_string = profile_path.value();
  remote_->OnProfileRemoved(ProfileHash(path_string));
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

void DeskProfilesLacros::SendProfileUpsert(const base::FilePath& profile_path) {
  if (auto* entry = profile_manager_->GetProfileAttributesStorage()
                        .GetProfileAttributesWithPath(profile_path)) {
    std::vector<mojom::LacrosProfileSummaryPtr> profiles;
    profiles.push_back(CreateProfileSummary(*entry));
    remote_->OnProfileUpsert(std::move(profiles));
  }
}

}  // namespace crosapi
