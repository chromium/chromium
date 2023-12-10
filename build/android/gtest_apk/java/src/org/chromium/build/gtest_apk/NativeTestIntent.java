// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.gtest_apk;

/** Extras for intent sent by NativeTestInstrumentationTestRunner. */
public class NativeTestIntent {
    public static final String EXTRA_COMMAND_LINE_FILE =
            "org.chromium.native_test.NativeTest.CommandLineFile";
    public static final String EXTRA_COMMAND_LINE_FLAGS =
            "org.chromium.native_test.NativeTest.CommandLineFlags";
    public static final String EXTRA_RUN_IN_SUB_THREAD =
            "org.chromium.native_test.NativeTest.RunInSubThread";
    public static final String EXTRA_GTEST_FILTER =
            "org.chromium.native_test.NativeTest.GtestFilter";
    public static final String EXTRA_STDOUT_FILE = "org.chromium.native_test.NativeTest.StdoutFile";
    public static final String EXTRA_COVERAGE_DEVICE_FILE =
            "org.chromium.native_test.NativeTest.CoverageDeviceFile";
}
