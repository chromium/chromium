// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.support.test.runner.AndroidJUnit4;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;

/**
 * Test class for {@link CommandLineInitUtil}.
 */
@RunWith(AndroidJUnit4.class)
public class CommandLineInitUtilTest {
    /**
     * Verifies that the default command line flags get set for Chrome Public tests.
     */
    @Test
    @SmallTest
    @Feature({"CommandLine"})
    public void testDefaultCommandLineFlagsSet() {
        CommandLineInitUtil.initCommandLine(CommandLineFlags.getTestCmdLineFile());
        Assert.assertTrue("CommandLine not initialized.", CommandLine.isInitialized());

        final CommandLine commandLine = CommandLine.getInstance();
        Assert.assertTrue(commandLine.hasSwitch("enable-test-intents"));
    }
}
