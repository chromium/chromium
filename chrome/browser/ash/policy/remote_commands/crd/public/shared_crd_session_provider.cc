// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session_provider.h"

#include <memory>

#include "ash/shell.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_admin_session_controller.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session.h"
#include "chrome/browser/ash/policy/remote_commands/crd/shared_crd_session_impl.h"
#include "components/prefs/pref_service.h"

namespace policy {

SharedCrdSessionProvider::SharedCrdSessionProvider(PrefService* local_state)
    : crd_controller_(std::make_unique<CrdAdminSessionController>()) {
  crd_controller_->Init(
      local_state,
      CHECK_DEREF(ash::Shell::Get()).security_curtain_controller());
}

SharedCrdSessionProvider::~SharedCrdSessionProvider() = default;

// TODO: crbug.com/403629500 - Refactor `CrdAdminSessionController` so it owns
// a `StartCrdSessionJobDelegate` instead of being one,
// so we don't need to pass in a full `CrdAdminSessionController` here,
// which does a bunch of things the `SharedCrdSession` doesn't want/care about.
std::unique_ptr<SharedCrdSession> SharedCrdSessionProvider::GetCrdSession() {
  return std::make_unique<SharedCrdSessionImpl>(crd_controller_->GetDelegate());
}
}  // namespace policy
