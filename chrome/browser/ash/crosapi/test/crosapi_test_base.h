// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_

#include <memory>

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
  explicit CrosapiTestBase(
      std::unique_ptr<AshCrosapiTestCommandLineModifierDelegate> delegate);
  CrosapiTestBase(const CrosapiTestBase&) = delete;
  CrosapiTestBase& operator=(const CrosapiTestBase&) = delete;
  ~CrosapiTestBase() override;

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
    (env_->GetCrosapiRemote().get()->*bind_func)(
        remote.BindNewPipeAndPassReceiver());
    return std::move(remote);
  }

  // A temp dir will be used as a user data dir.
  const base::FilePath& GetUserDataDir();

 private:
  std::unique_ptr<AshCrosapiTestEnv> env_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_
