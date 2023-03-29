// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test_crosapi_environment.h"

#include "base/check.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chromeos/ash/components/login/login_state/login_state.h"

namespace crosapi {

TestCrosapiEnvironment::TestCrosapiEnvironment() = default;

TestCrosapiEnvironment::~TestCrosapiEnvironment() = default;

void TestCrosapiEnvironment::SetUp() {
  // CrosapiAsh depends on ProfileManager.
  CHECK(testing_profile_manager_.SetUp());
  // Without this line, IdleServiceAsh gets initialized by CrosapiAsh and fails
  // due to missing dependencies.
  crosapi::IdleServiceAsh::DisableForTesting();
  // CrosapiAsh depends on LoginState. We initialize it here only if it hasn't
  // already been initialized by another test class such as AshTestBase.
  if (!ash::LoginState::IsInitialized()) {
    ash::LoginState::Initialize();
    initialized_login_state_ = true;
  }
  crosapi_manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();
}

void TestCrosapiEnvironment::TearDown() {
  testing_profile_manager_.DeleteAllTestingProfiles();
  crosapi_manager_.reset();
  if (initialized_login_state_) {
    ash::LoginState::Shutdown();
  }
}

}  // namespace crosapi
