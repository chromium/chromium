// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SERVER_BACKED_STATE_SERVER_BACKED_STATE_KEYS_BROKER_H_
#define CHROME_BROWSER_ASH_POLICY_SERVER_BACKED_STATE_SERVER_BACKED_STATE_KEYS_BROKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

namespace base {
class TimeDelta;
}

namespace policy {

// Brokers server-backed FRE state keys for the device. Retrieves them from
// session manager via D-Bus and refreshes them periodically. Consumers can
// register callbacks to invoke when the state keys change.
class ServerBackedStateKeysBroker {
 public:
  using UpdateCallbackList = base::RepeatingClosureList;
  using UpdateCallback = UpdateCallbackList::CallbackType;
  using ErrorType = ash::SessionManagerClient::StateKeyErrorType;
  using StateKeysCallbackList =
      base::OnceCallbackList<void(const std::vector<std::string>&)>;
  using StateKeysCallback = StateKeysCallbackList::CallbackType;

  explicit ServerBackedStateKeysBroker(
      ash::SessionManagerClient* session_manager_client);

  ServerBackedStateKeysBroker(const ServerBackedStateKeysBroker&) = delete;
  ServerBackedStateKeysBroker& operator=(const ServerBackedStateKeysBroker&) =
      delete;

  virtual ~ServerBackedStateKeysBroker();

  // Registers a callback to be invoked whenever the state keys get updated.
  // Note that consuming code needs to hold on to the returned subscription as
  // long as it wants to receive the callback. If the state keys haven't been
  // requested yet, calling this will also trigger their initial fetch.
  base::CallbackListSubscription RegisterUpdateCallback(
      const UpdateCallback& callback);

  // Requests state keys asynchronously. Invokes the passed callback at most
  // once, with the current state keys passed as a parameter to the callback. If
  // there's a problem determining the state keys, the passed vector will be
  // empty. If |this| gets destroyed before the callback happens or if the time
  // sync fails / the network is not established, then the |callback| is never
  // invoked. See http://crbug.com/649422 for more context.
  virtual void RequestStateKeys(StateKeysCallback callback);

  static base::TimeDelta GetPollIntervalForTesting();
  static base::TimeDelta GetRetryIntervalForTesting();

  // Get the set of current state keys. Empty if state keys are unavailable
  // or pending retrieval.
  const std::vector<std::string>& state_keys() const { return state_keys_; }

  // Returns latest state key retrieval error.
  virtual ErrorType error_type() const;

  // Returns the state key for the current point in time. Returns an empty
  // string if state keys are unavailable or pending retrieval.
  std::string current_state_key() const {
    return state_keys_.empty() ? std::string() : state_keys_.front();
  }

  // Whether state keys are available. Returns false if state keys are
  // unavailable or pending retrieval.
  bool available() const { return !state_keys_.empty(); }

 private:
  // Asks |session_manager_client_| to provide current state keys..
  void FetchStateKeys();

  // Stores newly-received state keys and notifies consumers.
  void StoreStateKeys(
      const base::expected<std::vector<std::string>, ErrorType>& state_keys);

  raw_ptr<ash::SessionManagerClient, DanglingUntriaged> session_manager_client_;

  // The current set of state keys.
  std::vector<std::string> state_keys_;
  ErrorType error_type_ = ErrorType::kNoError;

  // Whether a request for state keys is pending.
  bool requested_;

  // List of callbacks to receive update notifications.
  UpdateCallbackList update_callbacks_;

  // List of pending one-shot state key request callbacks.
  StateKeysCallbackList request_callbacks_;

  base::WeakPtrFactory<ServerBackedStateKeysBroker> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SERVER_BACKED_STATE_SERVER_BACKED_STATE_KEYS_BROKER_H_
