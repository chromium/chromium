// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/desk_profiles_ash.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/ranges/algorithm.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace crosapi {
namespace {

ash::LacrosProfileSummary ConvertProfileSummary(
    mojom::LacrosProfileSummaryPtr profile) {
  ash::LacrosProfileSummary result;
  result.profile_id = profile->profile_id;
  result.name = base::UTF8ToUTF16(profile->name);
  result.email = base::UTF8ToUTF16(profile->email);
  result.icon = std::move(profile->icon);

  return result;
}

}  // namespace

DeskProfilesAsh::DeskProfilesAsh() = default;
DeskProfilesAsh::~DeskProfilesAsh() = default;

void DeskProfilesAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DeskProfileObserver> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DeskProfilesAsh::OnProfileUpsert(
    std::vector<mojom::LacrosProfileSummaryPtr> profiles) {
  // Get the email of the primary user. This is done to figure out which summary
  // that we receive from lacros represents the primary user. We only need to do
  // this once.
  std::string primary_user_email;
  if (primary_user_profile_id_ == 0 && ash::Shell::HasInstance()) {
    if (const auto* session =
            ash::Shell::Get()->session_controller()->GetPrimaryUserSession()) {
      primary_user_email = session->user_info.account_id.GetUserEmail();
    }
  }

  for (auto& prof : profiles) {
    if (!primary_user_email.empty() &&
        gaia::AreEmailsSame(primary_user_email, prof->email)) {
      primary_user_profile_id_ = prof->profile_id;
    }

    auto& entry = UpsertProfile(ConvertProfileSummary(std::move(prof)));
    for (auto& observer : observers_) {
      observer.OnProfileUpsert(entry);
    }
  }
}

void DeskProfilesAsh::OnProfileRemoved(uint64_t profile_id) {
  if (RemoveProfile(profile_id)) {
    for (auto& observer : observers_) {
      observer.OnProfileRemoved(profile_id);
    }
  }
}

const std::vector<ash::LacrosProfileSummary>&
DeskProfilesAsh::GetProfilesSnapshot() const {
  return profiles_;
}

const ash::LacrosProfileSummary*
DeskProfilesAsh::GetProfilesSnapshotByProfileId(uint64_t profile_id) const {
  // Profile ID 0 is an alias for the primary user.
  if (profile_id == 0) {
    profile_id = primary_user_profile_id_;
  }

  for (auto& profile : profiles_) {
    if (profile.profile_id == profile_id) {
      return &profile;
    }
  }
  // If profile_id can't be found, return nullptr.
  return nullptr;
}

uint64_t DeskProfilesAsh::GetPrimaryProfileId() const {
  return primary_user_profile_id_;
}

void DeskProfilesAsh::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeskProfilesAsh::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ash::LacrosProfileSummary& DeskProfilesAsh::UpsertProfile(
    ash::LacrosProfileSummary&& summary) {
  auto existing = base::ranges::find(profiles_, summary.profile_id,
                                     &ash::LacrosProfileSummary::profile_id);
  // If we have this profile, update it.
  if (existing != profiles_.end()) {
    *existing = std::move(summary);
    return *existing;
  }

  // We don't have this profile, insert it.
  return profiles_.emplace_back(std::move(summary));
}

bool DeskProfilesAsh::RemoveProfile(uint64_t profile_id) {
  return std::erase_if(profiles_,
                       [profile_id](const ash::LacrosProfileSummary& summary) {
                         return summary.profile_id == profile_id;
                       }) != 0;
}

}  // namespace crosapi
