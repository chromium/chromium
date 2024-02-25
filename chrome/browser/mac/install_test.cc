// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// WARNING! Running these tests may alter or destroy a current installation of
// Google Chrome or GoogleUpdater.

namespace {

// Chromium does not contain GoogleUpdater; this test only makes sense in
// Chrome-branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

base::FilePath GetAppPath() {
  base::FilePath out_dir;
  if (!base::PathService::Get(base::DIR_EXE, &out_dir)) {
    return base::FilePath();
  }
  return out_dir.AppendASCII("Google Chrome.app");
}

TEST(InstallShTest, Install) {
  base::FilePath src = GetAppPath();
  base::ScopedTempDir dest;
  ASSERT_TRUE(dest.CreateUniqueTempDir());
  int exit_code = -1;
  std::string output;
  base::CommandLine command(
      {src.AppendASCII("Contents")
           .AppendASCII("Frameworks")
           .AppendASCII("Google Chrome Framework.framework")
           .AppendASCII("Resources")
           .AppendASCII("install.sh")
           .value(),
       src.value(), dest.GetPath().AppendASCII("Google Chrome.app").value()});
  EXPECT_TRUE(base::GetAppOutputWithExitCode(command, &output, &exit_code));
  EXPECT_EQ(exit_code, 0) << output;
  ASSERT_TRUE(
      base::PathExists(dest.GetPath().AppendASCII("Google Chrome.app")));
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

int main(int argc, char* argv[]) {
  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
