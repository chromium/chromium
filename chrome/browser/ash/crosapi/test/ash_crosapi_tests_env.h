// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_ASH_CROSAPI_TESTS_ENV_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_ASH_CROSAPI_TESTS_ENV_H_

#include "base/process/process.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

// This is for setting up tests environment for CrosapiTestBase.
class AshCrosapiTestEnv {
 public:
  AshCrosapiTestEnv();
  AshCrosapiTestEnv(const AshCrosapiTestEnv&) = delete;
  AshCrosapiTestEnv& operator=(const AshCrosapiTestEnv&) = delete;
  ~AshCrosapiTestEnv();

  static AshCrosapiTestEnv* GetInstance();

  mojo::Remote<mojom::Crosapi>& GetCrosapiRemote();

  // Returns if process and crosapi are valid.
  bool IsValid();

 private:
  mojo::Remote<mojom::Crosapi> crosapi_remote_;
  base::Process process_;
};
}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_ASH_CROSAPI_TESTS_ENV_H_
