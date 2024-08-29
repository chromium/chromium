// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests of {@link GSAUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class GSAUtilsUnitTest {

    @Test
    public void isAgsaVersionBelowMinimum() {
        Assert.assertFalse(GSAUtils.isAgsaVersionBelowMinimum("8.19", "8.19"));
        Assert.assertFalse(GSAUtils.isAgsaVersionBelowMinimum("8.19.1", "8.19"));
        Assert.assertFalse(GSAUtils.isAgsaVersionBelowMinimum("8.24", "8.19"));
        Assert.assertFalse(GSAUtils.isAgsaVersionBelowMinimum("8.25", "8.19"));
        Assert.assertFalse(GSAUtils.isAgsaVersionBelowMinimum("8.30", "8.19"));
        Assert.assertFalse(GSAUtils.isAgsaVersionBelowMinimum("9.30", "8.19"));

        Assert.assertTrue(GSAUtils.isAgsaVersionBelowMinimum("", "8.19"));
        Assert.assertTrue(GSAUtils.isAgsaVersionBelowMinimum("8.1", "8.19"));
        Assert.assertTrue(GSAUtils.isAgsaVersionBelowMinimum("7.30", "8.19"));
        Assert.assertTrue(GSAUtils.isAgsaVersionBelowMinimum("8", "8.19"));
    }
}
