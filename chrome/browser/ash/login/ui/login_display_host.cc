// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_host.h"

#include "base/functional/callback.h"

namespace ash {

// static
LoginDisplayHost* LoginDisplayHost::default_host_ = nullptr;

LoginDisplayHost::LoginDisplayHost() {
  DCHECK(default_host() == nullptr);
  default_host_ = this;
}

LoginDisplayHost::~LoginDisplayHost() {
  default_host_ = nullptr;
}

void LoginDisplayHost::AddWizardCreatedObserverForTests(
    base::RepeatingClosure on_created) {
  DCHECK(!on_wizard_controller_created_for_tests_);
  on_wizard_controller_created_for_tests_ = std::move(on_created);
}

void LoginDisplayHost::NotifyWizardCreated() {
  if (on_wizard_controller_created_for_tests_)
    on_wizard_controller_created_for_tests_.Run();
}

}  // namespace ash
