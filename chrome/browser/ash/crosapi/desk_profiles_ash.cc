// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/desk_profiles_ash.h"

#include <utility>

#include "base/containers/cxx20_erase_vector.h"
#include "base/ranges/algorithm.h"

namespace crosapi {
namespace {

ash::LacrosProfileSummary ConvertProfileSummary(
    mojom::LacrosProfileSummaryPtr profile) {
  ash::LacrosProfileSummary result;
  result.profile_id = profile->profile_id;
  result.name = std::move(profile->name);
  result.email = std::move(profile->email);
  result.icon = std::move(profile->icon);

  return result;
}

}  // namespace

DeskProfilesAsh::DeskProfilesAsh() = default;
DeskProfilesAsh::~DeskProfilesAsh() = default;

void DeskProfilesAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DeskProfileObserver> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&DeskProfilesAsh::OnDisconnect, base::Unretained(this)));
}

void DeskProfilesAsh::OnProfileUpsert(
    std::vector<mojom::LacrosProfileSummaryPtr> profiles) {
  for (auto& prof : profiles) {
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

std::vector<ash::LacrosProfileSummary> DeskProfilesAsh::GetProfilesSnapshot()
    const {
  return profiles_;
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
  return base::EraseIf(profiles_,
                       [profile_id](const ash::LacrosProfileSummary& summary) {
                         return summary.profile_id == profile_id;
                       }) != 0;
}

void DeskProfilesAsh::OnDisconnect() {
  receiver_.reset();
}

}  // namespace crosapi
