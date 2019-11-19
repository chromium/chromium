// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"

namespace policy {

namespace {

// Refresh interval for state keys. There's a quantized time component in
// state key generation, so they rotate over time. The quantum size is pretty
// coarse though (currently 2^23 seconds), so simply polling for a new state
// keys once a day is good enough.
constexpr base::TimeDelta kPollInterval = base::TimeDelta::FromDays(1);

}  // namespace

ServerBackedStateKeysBroker::ServerBackedStateKeysBroker(
    chromeos::SessionManagerClient* session_manager_client)
    : session_manager_client_(session_manager_client), requested_(false) {}

ServerBackedStateKeysBroker::~ServerBackedStateKeysBroker() {
}

ServerBackedStateKeysBroker::Subscription
ServerBackedStateKeysBroker::RegisterUpdateCallback(
    const base::RepeatingClosure& callback) {
  if (!available())
    FetchStateKeys();
  return update_callbacks_.Add(callback);
}

void ServerBackedStateKeysBroker::RequestStateKeys(StateKeysCallback callback) {
  if (!available()) {
    request_callbacks_.push_back(std::move(callback));
    FetchStateKeys();
    return;
  }

  if (!callback.is_null())
    std::move(callback).Run(state_keys_);
}

// static
base::TimeDelta ServerBackedStateKeysBroker::GetPollIntervalForTesting() {
  return kPollInterval;
}

void ServerBackedStateKeysBroker::FetchStateKeys() {
  if (!requested_) {
    requested_ = true;
    session_manager_client_->GetServerBackedStateKeys(
        base::Bind(&ServerBackedStateKeysBroker::StoreStateKeys,
                   weak_factory_.GetWeakPtr()));
  }
}

void ServerBackedStateKeysBroker::StoreStateKeys(
    const std::vector<std::string>& state_keys) {
  bool send_notification = !available();

  requested_ = false;
  if (state_keys.empty()) {
    LOG(WARNING) << "Failed to obtain server-backed state keys.";
  } else if (base::Contains(state_keys, std::string())) {
    LOG(WARNING) << "Bad state keys.";
  } else {
    send_notification |= state_keys_ != state_keys;
    state_keys_ = state_keys;
  }

  if (send_notification)
    update_callbacks_.Notify();

  std::vector<StateKeysCallback> callbacks;
  request_callbacks_.swap(callbacks);
  for (auto& callback : callbacks) {
    if (!callback.is_null())
      std::move(callback).Run(state_keys_);
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ServerBackedStateKeysBroker::FetchStateKeys,
                     weak_factory_.GetWeakPtr()),
      kPollInterval);
}

}  // namespace policy
