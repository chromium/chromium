// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/chrome_auth_parts.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"

namespace ash {

ChromeAuthParts::ChromeAuthParts(PrefService* local_state) {
  CHECK(local_state);
  auth_parts_ = AuthParts::Create(local_state);
  observation_.Observe(ash::SessionTerminationManager::Get());
}

ChromeAuthParts::~ChromeAuthParts() = default;

void ChromeAuthParts::OnAppTerminating() {
  observation_.Reset();
  AuthParts::Get()->Shutdown();
}

}  // namespace ash
