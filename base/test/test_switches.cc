// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_switches.h"

// Flag to show the help message.
const char switches::kHelpFlag[] = "help";

// Flag to run all tests and the launcher in a single process. Useful for
// debugging a specific test in a debugger.
const char switches::kSingleProcessTests[] = "single-process-tests";

// Maximum number of tests to run in a single batch.
const char switches::kTestLauncherBatchLimit[] = "test-launcher-batch-limit";

// Sets defaults desirable for the continuous integration bots, e.g. parallel
// test execution and test retries.
const char switches::kTestLauncherBotMode[] =
    "test-launcher-bot-mode";

// Makes it possible to debug the launcher itself. By default the launcher
// automatically switches to single process mode when it detects presence
// of debugger.
const char switches::kTestLauncherDebugLauncher[] =
    "test-launcher-debug-launcher";

// Force running all requested tests and retries even if too many test errors
// occur.
const char switches::kTestLauncherForceRunBrokenTests[] =
    "test-launcher-force-run-broken-tests";

// List of paths to files (separated by ';') containing test filters (one
// pattern per line).
const char switches::kTestLauncherFilterFile[] = "test-launcher-filter-file";

// Whether the test launcher should launch in "interactive mode", which disables
// timeouts (and may have other effects for specific test types).
const char switches::kTestLauncherInteractive[] = "test-launcher-interactive";

// Number of parallel test launcher jobs.
const char switches::kTestLauncherJobs[] = "test-launcher-jobs";

// Path to list of compiled in tests.
const char switches::kTestLauncherListTests[] = "test-launcher-list-tests";

// Path to test results file in our custom test launcher format.
const char switches::kTestLauncherOutput[] = "test-launcher-output";

// These two flags has the same effect, but don't use them at the same time.
// And isolated-script-test-launcher-retry-limit is preferred in the future.
// Maximum number of times to retry a test after failure.
const char switches::kTestLauncherRetryLimit[] = "test-launcher-retry-limit";
const char switches::kIsolatedScriptTestLauncherRetryLimit[] =
    "isolated-script-test-launcher-retry-limit";

// Path to test results file with all the info from the test launcher.
const char switches::kTestLauncherSummaryOutput[] =
    "test-launcher-summary-output";

// Flag controlling when test stdio is displayed as part of the launcher's
// standard output.
const char switches::kTestLauncherPrintTestStdio[] =
    "test-launcher-print-test-stdio";

// Print a writable path and exit (for internal use).
const char switches::kTestLauncherPrintWritablePath[] =
    "test-launcher-print-writable-path";

// Index of the test shard to run, starting from 0 (first shard) to total shards
// minus one (last shard).
const char switches::kTestLauncherShardIndex[] =
    "test-launcher-shard-index";

// Limit of test part results in the output. Default limit is 10.
// Negative value will completely disable limit.
const char switches::kTestLauncherTestPartResultsLimit[] =
    "test-launcher-test-part-results-limit";

// Total number of shards. Must be the same for all shards.
const char switches::kTestLauncherTotalShards[] =
    "test-launcher-total-shards";

// Time (in milliseconds) that the tests should wait before timing out.
const char switches::kTestLauncherTimeout[] = "test-launcher-timeout";

// Path where to save a trace of test launcher's execution.
const char switches::kTestLauncherTrace[] = "test-launcher-trace";

// TODO(phajdan.jr): Clean up the switch names.
const char switches::kTestTinyTimeout[] = "test-tiny-timeout";
const char switches::kUiTestActionTimeout[] = "ui-test-action-timeout";
const char switches::kUiTestActionMaxTimeout[] = "ui-test-action-max-timeout";

#if defined(OS_IOS)
// If enabled, runs unittests using the XCTest test runner.
const char switches::kEnableRunIOSUnittestsWithXCTest[] =
    "enable-run-ios-unittests-with-xctest";
#endif
