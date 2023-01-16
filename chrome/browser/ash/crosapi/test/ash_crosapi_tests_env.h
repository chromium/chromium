// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_ASH_CROSAPI_TESTS_ENV_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_ASH_CROSAPI_TESTS_ENV_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/process/process.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace crosapi {

// Delegate class to modify command line used for setting up ash process.
// To introduce ash env with customized command line, inherit this class to
// implement `AddExtraCommandLine`, and construct AshCrosapiTestEnv with
// passing the delegate class.
class AshCrosapiTestCommandLineModifierDelegate {
 public:
  AshCrosapiTestCommandLineModifierDelegate();
  AshCrosapiTestCommandLineModifierDelegate(
      const AshCrosapiTestCommandLineModifierDelegate&) = delete;
  AshCrosapiTestCommandLineModifierDelegate& operator=(
      const AshCrosapiTestCommandLineModifierDelegate&) = delete;
  virtual ~AshCrosapiTestCommandLineModifierDelegate();

  // Adds extra command line to `command_line`.
  // This is called before launching ash process, and the modified
  // `command_line` will be passed to ash process on launch.
  virtual void AddExtraCommandLine(base::CommandLine* command_line) = 0;
};

// AshCrosapiTestEnv is a test envirotnemtn for CrosapiTestBase.
// It's responsible for creating ash process and set up mojo connection.
class AshCrosapiTestEnv {
 public:
  AshCrosapiTestEnv();
  explicit AshCrosapiTestEnv(
      std::unique_ptr<AshCrosapiTestCommandLineModifierDelegate> delegate);
  AshCrosapiTestEnv(const AshCrosapiTestEnv&) = delete;
  AshCrosapiTestEnv& operator=(const AshCrosapiTestEnv&) = delete;
  ~AshCrosapiTestEnv();

  // Initializes Ash environment.
  void Initialize();

  // Returns true if process and crosapi are valid.
  bool IsValid();

  mojo::Remote<mojom::Crosapi>& GetCrosapiRemote() { return crosapi_remote_; }
  const base::FilePath& GetUserDataDir() { return user_data_dir_.GetPath(); }

 private:
  std::unique_ptr<AshCrosapiTestCommandLineModifierDelegate> delegate_;

  base::ScopedTempDir user_data_dir_;
  base::Process process_;
  mojo::Remote<mojom::Crosapi> crosapi_remote_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_ASH_CROSAPI_TESTS_ENV_H_
