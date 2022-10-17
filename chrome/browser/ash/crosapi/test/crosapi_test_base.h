// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_

#include "chrome/browser/ash/crosapi/test/ash_crosapi_tests_env.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class FilePath;
}

namespace crosapi {

// Base class for testing the behavior of crosapi on Ash-side only.
class CrosapiTestBase : public ::testing::Test {
 public:
  CrosapiTestBase();
  CrosapiTestBase(const CrosapiTestBase&) = delete;
  CrosapiTestBase& operator=(const CrosapiTestBase&) = delete;
  ~CrosapiTestBase() override;

  void SetUp() override;

 protected:
  // This function binds a remote for a given CrosapiInterface.
  // For example:
  //
  //   auto file_manager =
  //       BindCrosapiInterface(&mojom::Crosapi::BindFileManager);
  //
  //   file_manager->OpenFile(filepath, &result);
  //   EXPECT_EQ(crosapi::mojom::OpenResult::kFailedPathNotFound, result);
  //
  template <typename CrosapiInterface>
  mojo::Remote<CrosapiInterface> BindCrosapiInterface(void (
      mojom::Crosapi::*bind_func)(mojo::PendingReceiver<CrosapiInterface>)) {
    mojo::Remote<CrosapiInterface> remote;
    (AshCrosapiTestEnv::GetInstance()->GetCrosapiRemote().get()->*bind_func)(
        remote.BindNewPipeAndPassReceiver());
    return std::move(remote);
  }

  // A temp dir will be used as a user data dir.
  const base::FilePath& GetUserDataDir();
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_
