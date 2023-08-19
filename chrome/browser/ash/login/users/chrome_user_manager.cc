// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/chrome_user_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace ash {

ChromeUserManager::ChromeUserManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : UserManagerBase(
          std::move(task_runner),
          g_browser_process ? g_browser_process->local_state() : nullptr) {}

ChromeUserManager::~ChromeUserManager() = default;

// static
ChromeUserManager* ChromeUserManager::Get() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  return user_manager ? static_cast<ChromeUserManager*>(user_manager) : nullptr;
}

}  // namespace ash
