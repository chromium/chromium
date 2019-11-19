// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_SWITCHES_H_
#define BASE_TEST_TEST_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kHelpFlag[];
extern const char kSingleProcessTests[];
extern const char kTestLauncherBatchLimit[];
extern const char kTestLauncherBotMode[];
extern const char kTestLauncherDebugLauncher[];
extern const char kTestLauncherForceRunBrokenTests[];
extern const char kTestLauncherFilterFile[];
extern const char kTestLauncherInteractive[];
extern const char kTestLauncherJobs[];
extern const char kTestLauncherListTests[];
extern const char kTestLauncherOutput[];
extern const char kTestLauncherRetryLimit[];
extern const char kIsolatedScriptTestLauncherRetryLimit[];
extern const char kTestLauncherSummaryOutput[];
extern const char kTestLauncherPrintTestStdio[];
extern const char kTestLauncherPrintWritablePath[];
extern const char kTestLauncherShardIndex[];
extern const char kTestLauncherTestPartResultsLimit[];
extern const char kTestLauncherTotalShards[];
extern const char kTestLauncherTimeout[];
extern const char kTestLauncherTrace[];
extern const char kTestTinyTimeout[];
extern const char kUiTestActionTimeout[];
extern const char kUiTestActionMaxTimeout[];

#if defined(OS_IOS)
extern const char kEnableRunIOSUnittestsWithXCTest[];
#endif

}  // namespace switches

#endif  // BASE_TEST_TEST_SWITCHES_H_
