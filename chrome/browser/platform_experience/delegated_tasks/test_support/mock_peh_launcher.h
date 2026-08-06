// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_TEST_SUPPORT_MOCK_PEH_LAUNCHER_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_TEST_SUPPORT_MOCK_PEH_LAUNCHER_H_

#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace platform_experience {

class MockPehLauncher : public PehLauncher {
 public:
  MockPehLauncher();
  ~MockPehLauncher() override;

  MOCK_METHOD(base::FilePath, GetBinaryPath, (), (override));
  MOCK_METHOD(bool,
              IsBinaryVerified,
              (const base::FilePath& binary_path),
              (override));
  MOCK_METHOD(base::Version,
              GetBinaryVersion,
              (const base::FilePath& peh_binary_path),
              (override));
  MOCK_METHOD(base::Process,
              LaunchProcess,
              (const base::CommandLine& cmd_line,
               const base::LaunchOptions& options),
              (override));
};

}  // namespace platform_experience

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_TEST_SUPPORT_MOCK_PEH_LAUNCHER_H_
