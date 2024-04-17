// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

namespace policy {

namespace {

// Refresh interval for state keys. There's a quantized time component in
// state key generation, so they rotate over time. The quantum size is pretty
// coarse though (currently 2^23 seconds), so simply polling for a new state
// keys once a day is good enough.
constexpr base::TimeDelta kPollInterval = base::Days(1);

// In case state key fetching failed, we need to try again, sooner than
// |kPollInterval|.
constexpr base::TimeDelta kRetryInterval = base::Minutes(1);

}  // namespace

ServerBackedStateKeysBroker::ServerBackedStateKeysBroker(
    ash::SessionManagerClient* session_manager_client)
    : session_manager_client_(session_manager_client), requested_(false) {}

ServerBackedStateKeysBroker::~ServerBackedStateKeysBroker() {}

base::CallbackListSubscription
ServerBackedStateKeysBroker::RegisterUpdateCallback(
    const UpdateCallback& callback) {
  if (!available()) {
    LOG(WARNING) << "Fetching state keys.";
    FetchStateKeys();
  }
  return update_callbacks_.Add(callback);
}

void ServerBackedStateKeysBroker::RequestStateKeys(StateKeysCallback callback) {
  if (!available()) {
    request_callbacks_.AddUnsafe(std::move(callback));
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

base::TimeDelta ServerBackedStateKeysBroker::GetRetryIntervalForTesting() {
  return kRetryInterval;
}

void ServerBackedStateKeysBroker::FetchStateKeys() {
  if (!requested_) {
    requested_ = true;
    session_manager_client_->GetServerBackedStateKeys(
        base::BindOnce(&ServerBackedStateKeysBroker::StoreStateKeys,
                       weak_factory_.GetWeakPtr()));
  }
}

void ServerBackedStateKeysBroker::StoreStateKeys(
    const base::expected<std::vector<std::string>, ErrorType>& state_keys) {
  bool send_notification = !available();

  requested_ = false;
  auto wait_interval = kPollInterval;
  if (!state_keys.has_value()) {
    LOG(WARNING) << "Failed to obtain server-backed state keys. Error: "
                 << static_cast<int>(state_keys.error());
    wait_interval = kRetryInterval;
  } else {
    send_notification |= state_keys_ != state_keys.value();
  }
  state_keys_ = state_keys.value_or(std::vector<std::string>());
  error_type_ = state_keys.error_or(ErrorType::kNoError);

  if (send_notification)
    update_callbacks_.Notify();

  request_callbacks_.Notify(state_keys_);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ServerBackedStateKeysBroker::FetchStateKeys,
                     weak_factory_.GetWeakPtr()),
      wait_interval);
}

ServerBackedStateKeysBroker::ErrorType ServerBackedStateKeysBroker::error_type()
    const {
  return error_type_;
}

}  // namespace policy
