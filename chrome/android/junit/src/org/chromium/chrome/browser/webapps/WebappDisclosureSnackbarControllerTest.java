// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.webapk.lib.common.WebApkConstants;

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
        // Run AsyncTasks synchronously.
        PostTask.setPrenativeThreadPoolExecutorForTesting(new RoboExecutorService());

        doReturn("test text").when(mResources).getString(anyInt());
        doReturn(mManager).when(mActivity).getSnackbarManager();
        doReturn(mResources).when(mActivity).getResources();
    }

    private WebappDisclosureSnackbarController buildControllerForWebApk(String webApkPackageName) {
        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(webApkPackageName, "https://pwa.rocks/")
                        .build();
        return new WebappDisclosureSnackbarController(mActivity, intentDataProvider,
                mock(WebappDeferredStartupWithStorageHandler.class),
                mock(ActivityLifecycleDispatcher.class));
    }

    private WebappDataStorage registerStorageForWebApk(String packageName) {
        String id = WebappIntentUtils.getIdForWebApkPackage(packageName);
        WebappRegistry.getInstance().register(id, (storage) -> {});
        return WebappRegistry.getInstance().getWebappDataStorage(id);
    }

    public void verifyShownThenDismissedOnNewCreateStorage(String packageName) {
        WebappDisclosureSnackbarController controller = buildControllerForWebApk(packageName);
        WebappDataStorage storage = registerStorageForWebApk(packageName);

        // Simulates the case that shows the disclosure when creating a new storage.
        controller.onDeferredStartupWithStorage(storage, true /* didCreateStorage */);
        verify(mManager, times(1)).showSnackbar(any(Snackbar.class));
        assertTrue(storage.shouldShowDisclosure());

        // Simulate a restart or a resume.
        controller.onResumeWithNative();
        verify(mManager, times(2)).showSnackbar(any(Snackbar.class));
        assertTrue(storage.shouldShowDisclosure());

        // Dismiss the disclosure.
        controller.onAction(storage);

        // Simulate resuming or starting again this time no disclosure should show.
        assertFalse(storage.shouldShowDisclosure());
        controller.onResumeWithNative();
        verify(mManager, times(2)).showSnackbar(any(Snackbar.class));

        storage.delete();
    }

    public void verifyNotShownOnExistingStorageWithoutShouldShowDisclosure(String packageName) {
        WebappDisclosureSnackbarController controller = buildControllerForWebApk(packageName);
        WebappDataStorage storage = registerStorageForWebApk(packageName);

        // Simulate that starting with existing storage will not cause the disclosure to show.
        assertFalse(storage.shouldShowDisclosure());
        controller.onDeferredStartupWithStorage(storage, false /* didCreateStorage */);
        verify(mManager, times(0)).showSnackbar(any(Snackbar.class));

        storage.delete();
    }

    public void verifyNeverShown(String packageName) {
        WebappDisclosureSnackbarController controller = buildControllerForWebApk(packageName);
        WebappDataStorage storage = registerStorageForWebApk(packageName);

        // Try to show the disclosure the first time.
        controller.onDeferredStartupWithStorage(storage, true /* didCreateStorage */);
        verify(mManager, times(0)).showSnackbar(any(Snackbar.class));

        // Try to the disclosure again this time emulating a restart or a resume.
        controller.onResumeWithNative();
        verify(mManager, times(0)).showSnackbar(any(Snackbar.class));

        storage.delete();
    }

    @Test
    @Feature({"Webapps"})
    public void testUnboundWebApkShowDisclosure() {
        String packageName = "unbound";
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
        verifyNeverShown(packageName);
    }
}
