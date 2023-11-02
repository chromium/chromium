// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_crash_reporter_client_win.h"

#include <string>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(ChromeCrashReporterClientWin, GetWerModule) {
  ChromeCrashReporterClient client;

  std::wstring wer_module = client.GetWerRuntimeExceptionModule();
  ASSERT_FALSE(wer_module.empty());
  base::FilePath as_file_path(wer_module);
  ASSERT_EQ(as_file_path.Extension(), FILE_PATH_LITERAL(".dll"));
}

}  // namespace
