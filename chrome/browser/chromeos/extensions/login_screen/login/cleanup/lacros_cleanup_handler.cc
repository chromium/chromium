// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/lacros_cleanup_handler.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

LacrosCleanupHandler::LacrosCleanupHandler() = default;
LacrosCleanupHandler::~LacrosCleanupHandler() = default;

void LacrosCleanupHandler::Cleanup(CleanupHandlerCallback callback) {
  mojo::RemoteSet<crosapi::mojom::LacrosCleanupTriggeredObserver>* observers =
      GetCleanupTriggeredObservers();
  if (!has_set_disconnect_handlers_) {
    observers->set_disconnect_handler(base::BindRepeating(
        &LacrosCleanupHandler::OnDisconnect, base::Unretained(this)));
    has_set_disconnect_handlers_ = true;
  }

  callback_ = std::move(callback);
  errors_.clear();
  DCHECK(pending_observers_.empty());

  if (observers->empty()) {
    OnAllObserversDone();
    return;
  }

  // Add `pending_observers_` in a separate loop as observers might finish
  // synchronously.
  for (auto it = observers->begin(); it != observers->end(); ++it) {
    mojo::RemoteSetElementId id = it.id();
    pending_observers_.emplace(id);
  }

  for (auto it = observers->begin(); it != observers->end(); ++it) {
    mojo::RemoteSetElementId id = it.id();
    (*it)->OnLacrosCleanupTriggered(base::BindOnce(
        &LacrosCleanupHandler::OnObserverDone, base::Unretained(this), id));
  }
}

void LacrosCleanupHandler::SetCleanupTriggeredObserversForTesting(
    mojo::RemoteSet<crosapi::mojom::LacrosCleanupTriggeredObserver>*
        observers_for_testing) {
  observers_for_testing_ = observers_for_testing;
}

void LacrosCleanupHandler::OnDisconnect(mojo::RemoteSetElementId id) {
  // Observer might have already finished before disconnecting.
  if (pending_observers_.find(id) == pending_observers_.end())
    return;

  pending_observers_.erase(id);
  if (pending_observers_.empty())
    OnAllObserversDone();
}

void LacrosCleanupHandler::OnObserverDone(
    mojo::RemoteSetElementId id,
    const std::optional<std::string>& error) {
  // Sanity check - observers should have flushed pending messages before
  // disconnecting.
  if (pending_observers_.find(id) == pending_observers_.end())
    return;

  if (error)
    errors_.push_back(*error);

  pending_observers_.erase(id);
  if (pending_observers_.empty())
    OnAllObserversDone();
}

void LacrosCleanupHandler::OnAllObserversDone() {
  if (errors_.empty()) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  std::string errors = base::JoinString(errors_, "\n");
  std::move(callback_).Run(errors);
}

mojo::RemoteSet<crosapi::mojom::LacrosCleanupTriggeredObserver>*
LacrosCleanupHandler::GetCleanupTriggeredObservers() {
  if (observers_for_testing_)
    return observers_for_testing_;

  return &crosapi::CrosapiManager::Get()
              ->crosapi_ash()
              ->login_ash()
              ->GetCleanupTriggeredObservers();
}

}  // namespace chromeos
