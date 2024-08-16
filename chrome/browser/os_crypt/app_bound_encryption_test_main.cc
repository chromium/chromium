// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "chrome/browser/os_crypt/test_support.h"
#include "chrome/install_static/test/scoped_install_details.h"

namespace os_crypt {

namespace {

// See the switch definitions in test_support.cc for the arguments used to
// control the behavior of this function.
HRESULT ExecuteTest(const base::CommandLine& cmd_line) {
  install_static::ScopedInstallDetails scoped_install_details_(
      std::make_unique<FakeInstallDetails>());

  auto in_file =
      cmd_line.GetSwitchValuePath(switches::kAppBoundTestInputFilename);
  CHECK(!in_file.empty()) << "Input file must be specified.";
  std::string input_data;
  CHECK(base::ReadFileToString(in_file, &input_data))
      << "Cannot read input file.";

  // The test data header prevents the test executable ever being used to
  // encrypt or decrypt production data.
  const std::string kTestHeader("TESTDATAHEADER");

  std::string output_data;
  DWORD last_error;
  HRESULT hr = S_FALSE;

  if (cmd_line.HasSwitch(switches::kAppBoundTestModeEncrypt)) {
    input_data.insert(0, kTestHeader);
    hr = EncryptAppBoundString(ProtectionLevel::PROTECTION_PATH_VALIDATION,
                               input_data, output_data, last_error);
  } else if (cmd_line.HasSwitch(switches::kAppBoundTestModeDecrypt)) {
    hr = DecryptAppBoundString(input_data, output_data, last_error);
    if (SUCCEEDED(hr)) {
      CHECK_EQ(output_data.compare(0, kTestHeader.length(), kTestHeader), 0);
      output_data.erase(0, kTestHeader.length());
    }
  } else {
    NOTREACHED() << "A valid mode must be specified";
  }

  if (SUCCEEDED(hr)) {
    auto out_file =
        cmd_line.GetSwitchValuePath(switches::kAppBoundTestOutputFilename);
    CHECK(!out_file.empty()) << "Output file must be specified.";
    CHECK(base::WriteFile(out_file, output_data))
        << "Cannot write output data.";
  }
  return hr;
}

}  // namespace

}  // namespace os_crypt

int main(int, char**) {
  base::AtExitManager exit_manager;
  base::win::ScopedCOMInitializer com_initializer;

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  InitLogging(settings);

  CHECK(base::CommandLine::Init(0, nullptr));

  return static_cast<int>(
      os_crypt::ExecuteTest(*base::CommandLine::ForCurrentProcess()));
}
