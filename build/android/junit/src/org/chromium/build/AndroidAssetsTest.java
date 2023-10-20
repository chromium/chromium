// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.io.IOException;
import java.io.InputStream;

/** Checks that Robolectric tests can use android assets. */
@RunWith(RobolectricTestRunner.class)
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
    public void testAssetsExist() throws IOException {
        String myselfAsAssetData = readTestAsset();
        Assert.assertTrue(
                "asset not correct. It had length=" + myselfAsAssetData.length(),
                myselfAsAssetData.contains("String myselfAsAssetData = "));
    }

    @Test
    public void testResourcesExist() {
        String actual = RuntimeEnvironment.getApplication().getString(R.string.test_string);
        Assert.assertEquals("Hello World", actual);
    }

    @Test
    public void testManifestMerged() throws NameNotFoundException {
        Context context = RuntimeEnvironment.getApplication();
        ApplicationInfo info =
                context.getPackageManager()
                        .getApplicationInfo(context.getPackageName(), PackageManager.GET_META_DATA);
        String actual = info.metaData.getString("test-metadata");
        Assert.assertEquals("Hello World", actual);
    }
}
