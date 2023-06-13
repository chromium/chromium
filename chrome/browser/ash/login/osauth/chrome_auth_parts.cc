// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/chrome_auth_parts.h"

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"

namespace ash {

ChromeAuthParts::ChromeAuthParts() {
  auth_parts_ = AuthParts::Create(g_browser_process->local_state());
  app_termination_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &ChromeAuthParts::OnAppTerminating, base::Unretained(this)));
}

ChromeAuthParts::~ChromeAuthParts() = default;

void ChromeAuthParts::OnAppTerminating() {
  AuthParts::Get()->Shutdown();
}

}  // namespace ash
