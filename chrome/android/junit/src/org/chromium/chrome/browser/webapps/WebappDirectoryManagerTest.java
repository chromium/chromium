// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Feature;

import java.io.File;

/** Tests that directories for WebappActivities are managed correctly. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebappDirectoryManagerTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.getApplication();
        ContextUtils.initApplicationContext(mContext);
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        PathUtils.setPrivateDataDirectorySuffix("chrome");
        WebappDirectoryManager.resetForTesting();
    }

    @Test
    @Feature({"Webapps"})
    public void testDeletesObsoleteDirectories() {
        // Seed the base directory with folders that correspond to pre-L web apps.
        File baseDirectory = mContext.getDataDir();
        File webappDirectory = new File(baseDirectory, "app_WebappActivity");
        File nonWebappDirectory = new File(baseDirectory, "app_ChromeDocumentActivity");
        Assert.assertTrue(webappDirectory.mkdirs());
        Assert.assertTrue(nonWebappDirectory.mkdirs());

        // Make sure only the web app folders are deleted.
        runCleanup();
        Assert.assertFalse(webappDirectory.exists());
        Assert.assertTrue(nonWebappDirectory.exists());

        Assert.assertTrue(webappDirectory.mkdirs());
        // Make sure the second cleanup call no-ops.
        runCleanup();
        Assert.assertTrue(webappDirectory.exists());
        Assert.assertTrue(nonWebappDirectory.exists());
    }

    private void runCleanup() {
        WebappDirectoryManager.cleanUpDirectories();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
    }
}
