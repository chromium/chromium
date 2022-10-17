// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test/crosapi_test_base.h"

#include "chrome/browser/ash/crosapi/test/ash_crosapi_tests_env.h"

namespace crosapi {

CrosapiTestBase::CrosapiTestBase() = default;
CrosapiTestBase::~CrosapiTestBase() = default;

void CrosapiTestBase::SetUp() {
  ASSERT_TRUE(AshCrosapiTestEnv::GetInstance()->IsValid());
}

const base::FilePath& CrosapiTestBase::GetUserDataDir() {
  return AshCrosapiTestEnv::GetInstance()->GetUserDataDir();
}
}  // namespace crosapi
