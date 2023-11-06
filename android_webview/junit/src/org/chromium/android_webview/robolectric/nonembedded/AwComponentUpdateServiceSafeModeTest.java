// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.nonembedded;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.nonembedded.AwComponentUpdateService;
import org.chromium.android_webview.services.ComponentUpdaterResetSafeModeAction;
import org.chromium.base.FileUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.File;
import java.io.IOException;
import java.util.HashSet;
import java.util.Set;

/** Test AwComponentUpdateService's behavior when Safe Mode Reset is applied. */
@RunWith(BaseRobolectricTestRunner.class)
public class AwComponentUpdateServiceSafeModeTest {
    private static final String TEST_FILE = "testManifest.json";
    private File mDirectory;
    private AwComponentUpdateService mComponentUpdateService;

    @Mock private SafeModeController mMockSafeModeController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mComponentUpdateService = new AwComponentUpdateService();
        PathUtils.setPrivateDataDirectorySuffix("webview", "WebView");
        mDirectory = new File(PathUtils.getDataDirectory(), "components/cus/");
        if (!mDirectory.exists()) {
            Assert.assertTrue(mDirectory.mkdirs());
        }

        mMockSafeModeController = mock(SafeModeController.class);
        SafeModeController.setInstanceForTests(mMockSafeModeController);
    }

    @After
    public void tearDown() {
        Assert.assertTrue(
                "Failed to delete " + mDirectory.getAbsolutePath(),
                FileUtils.recursivelyDeleteFile(mDirectory, null));
    }

    @Test
    @SmallTest
    public void testComponentUpdaterResetDeletesDownloadedConfigs() throws IOException {
        File cusFile = new File(mDirectory, TEST_FILE);
        Assert.assertTrue(
                "Failed to create test file " + cusFile.getAbsolutePath(), cusFile.createNewFile());

        final String componentUpdaterResetActionId =
                new ComponentUpdaterResetSafeModeAction().getId();
        Set<String> actions = new HashSet<>();
        actions.add(componentUpdaterResetActionId);
        when(mMockSafeModeController.queryActions(anyString())).thenReturn(actions);
        when(mMockSafeModeController.isSafeModeEnabled(anyString())).thenReturn(true);

        Assert.assertFalse(mComponentUpdateService.maybeStartUpdates(false));

        File[] cusFiles = mDirectory.listFiles();
        Assert.assertNull(cusFiles);
    }
}
