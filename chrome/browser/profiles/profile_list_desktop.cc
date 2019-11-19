// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_list_desktop.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ProfileListDesktop::ProfileListDesktop(
    ProfileAttributesStorage* profile_storage)
    : profile_storage_(profile_storage) {
}

ProfileListDesktop::~ProfileListDesktop() {
}

// static
ProfileList* ProfileList::Create(ProfileAttributesStorage* profile_storage) {
  return new ProfileListDesktop(profile_storage);
}

size_t ProfileListDesktop::GetNumberOfItems() const {
  return items_.size();
}

const AvatarMenu::Item& ProfileListDesktop::GetItemAt(size_t index) const {
  DCHECK_LT(index, items_.size());
  return *items_[index];
}

void ProfileListDesktop::RebuildMenu() {
  std::vector<ProfileAttributesEntry*> entries =
      profile_storage_->GetAllProfilesAttributesSortedByName();

  items_.clear();
  for (ProfileAttributesEntry* entry : entries) {
    if (entry->IsOmitted())
      continue;

    gfx::Image icon = entry->GetAvatarIcon();
    std::unique_ptr<AvatarMenu::Item> item(
        new AvatarMenu::Item(items_.size(), entry->GetPath(), icon));
    item->name = entry->GetName();
    item->username = entry->GetUserName();
    item->legacy_supervised = entry->IsLegacySupervised();
    item->child_account = entry->IsChild();
    item->signed_in = entry->IsAuthenticated();
    if (entry->GetSigninState() == SigninState::kNotSignedIn) {
      item->username = l10n_util::GetStringUTF16(
          item->legacy_supervised ? IDS_LEGACY_SUPERVISED_USER_AVATAR_LABEL :
                                    IDS_PROFILES_LOCAL_PROFILE_STATE);
    }
    item->active = item->profile_path == active_profile_path_;
    item->signin_required = entry->IsSigninRequired();
    items_.push_back(std::move(item));
  }
}

size_t ProfileListDesktop::MenuIndexFromProfilePath(const base::FilePath& path)
    const {
  const size_t menu_count = GetNumberOfItems();

  for (size_t i = 0; i < menu_count; ++i) {
    const AvatarMenu::Item item = GetItemAt(i);
    if (item.profile_path == path)
      return i;
  }

  // The desired index was not found; return a fallback value.
  NOTREACHED();
  return 0;
}

void ProfileListDesktop::ActiveProfilePathChanged(
    const base::FilePath& active_profile_path) {
  active_profile_path_ = active_profile_path;
}
