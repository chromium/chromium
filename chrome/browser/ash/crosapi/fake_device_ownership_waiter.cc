// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/fake_device_ownership_waiter.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

void FakeDeviceOwnershipWaiter::WaitForOwnerhipFetched(
    base::OnceClosure callback,
    bool launching_at_login_screen) {
  if (launching_at_login_screen ||
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
      profiles::IsDemoSession()) {
    std::move(callback).Run();
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

}  // namespace crosapi
