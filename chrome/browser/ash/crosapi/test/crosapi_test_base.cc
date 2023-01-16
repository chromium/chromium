// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test/crosapi_test_base.h"

namespace crosapi {

CrosapiTestBase::CrosapiTestBase() : CrosapiTestBase(nullptr) {}

CrosapiTestBase::CrosapiTestBase(
    std::unique_ptr<AshCrosapiTestCommandLineModifierDelegate> delegate)
    : env_(std::make_unique<AshCrosapiTestEnv>(std::move(delegate))) {}

CrosapiTestBase::~CrosapiTestBase() = default;

const base::FilePath& CrosapiTestBase::GetUserDataDir() {
  return env_->GetUserDataDir();
}
}  // namespace crosapi
