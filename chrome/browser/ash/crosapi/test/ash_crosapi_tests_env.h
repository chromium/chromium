// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_ASH_CROSAPI_TESTS_ENV_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_ASH_CROSAPI_TESTS_ENV_H_

#include "base/files/scoped_temp_dir.h"
#include "base/process/process.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class FilePath;
}

namespace crosapi {

// This is for setting up tests environment for CrosapiTestBase.
class AshCrosapiTestEnv {
 public:
  AshCrosapiTestEnv();
  AshCrosapiTestEnv(const AshCrosapiTestEnv&) = delete;
  AshCrosapiTestEnv& operator=(const AshCrosapiTestEnv&) = delete;
  ~AshCrosapiTestEnv();

  static AshCrosapiTestEnv* GetInstance();

  mojo::Remote<mojom::Crosapi>& GetCrosapiRemote() { return crosapi_remote_; }
  const base::FilePath& GetUserDataDir() { return user_data_dir_.GetPath(); }

  // Returns if process and crosapi are valid.
  bool IsValid();

 private:
  base::ScopedTempDir user_data_dir_;
  base::Process process_;
  mojo::Remote<mojom::Crosapi> crosapi_remote_;
};
}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_ASH_CROSAPI_TESTS_ENV_H_
