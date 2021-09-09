// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/field_trial_service_ash.h"

namespace {
crosapi::mojom::FieldTrialGroupInfoPtr CreateFieldTrialGroupInfo(
    const std::string& trial_name,
    const std::string& group_name) {
  auto info = crosapi::mojom::FieldTrialGroupInfo::New();
  info->trial_name = trial_name;
  info->group_name = group_name;
  return info;
}
}  // namespace

namespace crosapi {

FieldTrialServiceAsh::FieldTrialServiceAsh() {
  base::FieldTrialList::AddObserver(this);
}

FieldTrialServiceAsh::~FieldTrialServiceAsh() {
  base::FieldTrialList::RemoveObserver(this);
}

void FieldTrialServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::FieldTrialService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FieldTrialServiceAsh::AddFieldTrialObserver(
    mojo::PendingRemote<mojom::FieldTrialObserver> observer) {
  mojo::Remote<mojom::FieldTrialObserver> remote(std::move(observer));

  // Send active field trial groups when the observer is added
  // before subscribing field trial group updates.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  std::vector<mojom::FieldTrialGroupInfoPtr> infos;
  for (const auto& group : active_groups) {
    infos.push_back(
        CreateFieldTrialGroupInfo(group.trial_name, group.group_name));
  }
  remote->OnFieldTrialGroupActivated(std::move(infos));

  observers_.Add(std::move(remote));
}

void FieldTrialServiceAsh::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  for (auto& observer : observers_) {
    std::vector<mojom::FieldTrialGroupInfoPtr> infos;
    infos.push_back(CreateFieldTrialGroupInfo(trial_name, group_name));
    observer->OnFieldTrialGroupActivated(std::move(infos));
  }
}

}  // namespace crosapi
