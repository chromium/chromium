// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.webapk.lib.common.WebApkConstants;

/**
 * Tests for WebappDisclosureSnackbarController
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebappDisclosureSnackbarControllerTest {
    @Mock
    public WebappActivity mActivity;
    @Mock
    public SnackbarManager mManager;
    @Mock
    public Resources mResources;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn("test text").when(mResources).getString(anyInt());
        doReturn(mManager).when(mActivity).getSnackbarManager();
        doReturn(mResources).when(mActivity).getResources();
    }

    public void verifyShownThenDismissedOnNewCreateStorage(String packageName) {
        WebappDisclosureSnackbarController controller = new WebappDisclosureSnackbarController();
        WebappDataStorage storage = WebappDataStorage.open(packageName);

        // Simulates the case that shows the disclosure when creating a new storage.
        controller.maybeShowDisclosure(mActivity, storage, true);
        verify(mManager, times(1)).showSnackbar(any(Snackbar.class));
        assertTrue(storage.shouldShowDisclosure());

        // Simulate a restart or a resume (has storage so `force = false`).
        controller.maybeShowDisclosure(mActivity, storage, false);
        verify(mManager, times(2)).showSnackbar(any(Snackbar.class));
        assertTrue(storage.shouldShowDisclosure());

        // Dismiss the disclosure.
        controller.onAction(storage);

        // Simulate resuming or starting again this time no disclosure should show.
        assertFalse(storage.shouldShowDisclosure());
        controller.maybeShowDisclosure(mActivity, storage, false);
        verify(mManager, times(2)).showSnackbar(any(Snackbar.class));

        storage.delete();
    }

    public void verifyNotShownOnExistingStorageWithoutShouldShowDisclosure(String packageName) {
        WebappDisclosureSnackbarController controller = new WebappDisclosureSnackbarController();
        WebappDataStorage storage = WebappDataStorage.open(packageName);

        // Simulate that starting with existing storage will not cause the disclosure to show.
        assertFalse(storage.shouldShowDisclosure());
        controller.maybeShowDisclosure(mActivity, storage, false);
        verify(mManager, times(0)).showSnackbar(any(Snackbar.class));

        storage.delete();
    }

    public void verifyNeverShown(String packageName) {
        WebappDisclosureSnackbarController controller = new WebappDisclosureSnackbarController();
        WebappDataStorage storage = WebappDataStorage.open(packageName);

        // Try to show the disclosure the first time (fake having no storage on startup by setting
        // `force = true`) this shouldn't work as the app was installed via Chrome.
        controller.maybeShowDisclosure(mActivity, storage, true);
        verify(mManager, times(0)).showSnackbar(any(Snackbar.class));

        // Try to the disclosure again this time emulating a restart or a resume (fake having
        // storage `force = false`) again this shouldn't work.
        controller.maybeShowDisclosure(mActivity, storage, false);
        verify(mManager, times(0)).showSnackbar(any(Snackbar.class));

        storage.delete();
    }

    @Test
    @Feature({"Webapps"})
    public void testUnboundWebApkShowDisclosure() {
        String packageName = "unbound";
        doReturn(packageName).when(mActivity).getWebApkPackageName();

        verifyShownThenDismissedOnNewCreateStorage(packageName);
    }

    @Test
    @Feature({"Webapps"})
    public void testUnboundWebApkNoDisclosureOnExistingStorage() {
        verifyNotShownOnExistingStorageWithoutShouldShowDisclosure("unbound");
    }

    @Test
    @Feature({"Webapps"})
    public void testBoundWebApkNoDisclosure() {
        String packageName = WebApkConstants.WEBAPK_PACKAGE_PREFIX + ".bound";
        doReturn(packageName).when(mActivity).getWebApkPackageName();

        verifyNeverShown(packageName);
    }

    @Test
    @Feature({"Webapps"})
    public void testWebappNoDisclosure() {
        String packageName = "webapp";
        // Don't set a client package name, it should be null for Webapps.

        verifyNeverShown(packageName);
    }
}
