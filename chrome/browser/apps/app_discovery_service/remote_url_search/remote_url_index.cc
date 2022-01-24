// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_index.h"

#include "base/time/time.h"
#include "base/values.h"

namespace apps {
namespace {

constexpr base::TimeDelta kUpdateInterval = base::Hours(24);

}  // namespace

RemoteUrlIndex::RemoteUrlIndex(std::unique_ptr<RemoteUrlClient> client,
                               const base::FilePath& storage_path)
    : client_(std::move(client)), storage_path_(storage_path) {
  MaybeUpdateAndReschedule();
}

RemoteUrlIndex::~RemoteUrlIndex() = default;

base::Value* RemoteUrlIndex::GetApps(const std::string& query) {
  // TODO(crbug.com/1244221): Unimplemented.
  return nullptr;
}

void RemoteUrlIndex::MaybeUpdateAndReschedule() {
  client_->Fetch(base::BindOnce(&RemoteUrlIndex::OnUpdateComplete,
                                weak_factory_.GetWeakPtr()));

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RemoteUrlIndex::MaybeUpdateAndReschedule,
                     weak_factory_.GetWeakPtr()),
      kUpdateInterval);
}

void RemoteUrlIndex::OnUpdateComplete(RemoteUrlClient::Status status,
                                      base::Value value) {
  // TODO(crbug.com/1244221): Unimplemented.
}

}  // namespace apps
