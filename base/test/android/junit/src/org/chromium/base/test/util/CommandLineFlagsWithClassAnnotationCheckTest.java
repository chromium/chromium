// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link CommandLineFlags} annotations. This is for testing what happens when a flag
 * is added at the class level.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add("some-switch")
public class CommandLineFlagsWithClassAnnotationCheckTest {
    @Test
    public void testOnlyClassAnnotation() throws Throwable {
        Assert.assertTrue(
                "some-switch should be appended by the class",
                CommandLine.getInstance().hasSwitch("some-switch"));
    }

    @Test
    @CommandLineFlags.Remove("some-switch")
    public void testRemoveSwitch_method() throws Throwable {
        Assert.assertTrue(
                "CommandLine switches should be removed by the method",
                CommandLine.getInstance().getSwitches().isEmpty());
    }
}
