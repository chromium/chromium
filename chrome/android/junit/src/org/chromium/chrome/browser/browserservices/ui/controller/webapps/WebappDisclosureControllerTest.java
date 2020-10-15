// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE_DISMISSED_BY_USER;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;

import org.junit.After;
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
import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappDeferredStartupWithStorageHandler;
import org.chromium.chrome.browser.webapps.WebappIntentUtils;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.webapk.lib.common.WebApkConstants;

/**
 * Tests for WebappDisclosureController
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebappDisclosureControllerTest {
    @Mock
    public WebappActivity mActivity;

    public TrustedWebActivityModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        // Run AsyncTasks synchronously.
        PostTask.setPrenativeThreadPoolExecutorForTesting(new RoboExecutorService());
    }

    @After
    public void tearDown() {
        PostTask.resetPrenativeThreadPoolExecutorForTesting();
    }

    private WebappDisclosureController buildControllerForWebApk(String webApkPackageName) {
        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(webApkPackageName, "https://pwa.rocks/")
                        .build();
        mModel = new TrustedWebActivityModel();
        return new WebappDisclosureController(mActivity, intentDataProvider, mModel,
                mock(WebappDeferredStartupWithStorageHandler.class),
                mock(ActivityLifecycleDispatcher.class));
    }

    private WebappDataStorage registerStorageForWebApk(String packageName) {
        String id = WebappIntentUtils.getIdForWebApkPackage(packageName);
        WebappRegistry.getInstance().register(id, (storage) -> {});
        return WebappRegistry.getInstance().getWebappDataStorage(id);
    }

    public void verifyShownThenDismissedOnNewCreateStorage(String packageName) {
        WebappDisclosureController controller = buildControllerForWebApk(packageName);
        WebappDataStorage storage = registerStorageForWebApk(packageName);

        // Simulates the case that shows the disclosure when creating a new storage.
        controller.onDeferredStartupWithStorage(storage, true /* didCreateStorage */);
        assertTrue(storage.shouldShowDisclosure());
        assertSnackbarShown();

        // Dismiss the disclosure.
        mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureAccepted();

        assertSnackbarAccepted();
        assertFalse(storage.shouldShowDisclosure());

        storage.delete();
    }

    public void verifyShownThenDismissedOnRestart(String packageName) {
        WebappDisclosureController controller = buildControllerForWebApk(packageName);
        WebappDataStorage storage = registerStorageForWebApk(packageName);

        // Simulates the case that shows the disclosure when finish native initialization.
        storage.setShowDisclosure();
        controller.onFinishNativeInitialization();
        assertSnackbarShown();

        // Dismiss the disclosure.
        mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureAccepted();

        assertSnackbarAccepted();
        assertFalse(storage.shouldShowDisclosure());

        storage.delete();
    }

    public void verifyNotShownOnExistingStorageWithoutShouldShowDisclosure(String packageName) {
        WebappDisclosureController controller = buildControllerForWebApk(packageName);
        WebappDataStorage storage = registerStorageForWebApk(packageName);

        // Simulate that starting with existing storage will not cause the disclosure to show.
        assertFalse(storage.shouldShowDisclosure());
        controller.onDeferredStartupWithStorage(storage, false /* didCreateStorage */);
        assertSnackbarNotShown();

        storage.delete();
    }

    public void verifyNeverShown(String packageName) {
        WebappDisclosureController controller = buildControllerForWebApk(packageName);
        WebappDataStorage storage = registerStorageForWebApk(packageName);

        // Try to show the disclosure the first time.
        controller.onDeferredStartupWithStorage(storage, true /* didCreateStorage */);
        assertSnackbarNotShown();

        // Try to the disclosure again this time emulating a restart.
        controller.onFinishNativeInitialization();
        assertSnackbarNotShown();

        storage.delete();
    }

    private void assertSnackbarShown() {
        assertEquals(DISCLOSURE_STATE_SHOWN, mModel.get(DISCLOSURE_STATE));
    }

    private void assertSnackbarAccepted() {
        assertEquals(DISCLOSURE_STATE_DISMISSED_BY_USER, mModel.get(DISCLOSURE_STATE));
    }

    private void assertSnackbarNotShown() {
        assertEquals(DISCLOSURE_STATE_NOT_SHOWN, mModel.get(DISCLOSURE_STATE));
    }

    @Test
    @Feature({"Webapps"})
    public void testUnboundWebApkShowDisclosure() {
        String packageName = "unbound";
        verifyShownThenDismissedOnNewCreateStorage(packageName);
    }

    @Test
    @Feature({"Webapps"})
    public void testUnboundWebApkShowDisclosure2() {
        String packageName = "unbound";
        verifyShownThenDismissedOnRestart(packageName);
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
