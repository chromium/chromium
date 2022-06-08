// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.io.IOException;
import java.io.InputStream;

/**
 * Checks that Robolectric tests can use android assets.
 */
@RunWith(LocalRobolectricTestRunner.class)
public class AndroidAssetsTest {
    private static final String TEST_ASSET_NAME = "AndroidAssetsTest.java";

    public String readTestAsset() throws IOException {
        try (InputStream stream =
                        RuntimeEnvironment.getApplication().getAssets().open(TEST_ASSET_NAME)) {
            byte[] buffer = new byte[stream.available()];
            stream.read(buffer);
            return new String(buffer);
        }
    }
    @Test
    public void testTestStarted() throws IOException {
        String myselfAsAssetData = readTestAsset();
        Assert.assertTrue("asset not correct. It had length=" + myselfAsAssetData.length(),
                myselfAsAssetData.contains("testTestStarted()"));
    }
}
