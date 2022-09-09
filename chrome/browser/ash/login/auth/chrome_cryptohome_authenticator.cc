// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/auth/chrome_cryptohome_authenticator.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/auth/chrome_safe_mode_delegate.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/ownership/owner_key_util.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

using ::content::BrowserThread;

ChromeCryptohomeAuthenticator::ChromeCryptohomeAuthenticator(
    AuthStatusConsumer* consumer)
    : CryptohomeAuthenticator(base::ThreadTaskRunnerHandle::Get(),
                              g_browser_process->local_state(),
                              std::make_unique<ChromeSafeModeDelegate>(),
                              consumer) {}

ChromeCryptohomeAuthenticator::~ChromeCryptohomeAuthenticator() = default;

}  // namespace ash
