// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_DISMISSED_BY_USER;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationState;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappDeferredStartupWithStorageHandler;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.webapk.lib.common.WebApkConstants;

/** Tests for WebappDisclosureController */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// TODO(crbug.com/40182398): Change to use paused looper. See crbug for details.
@LooperMode(LooperMode.Mode.LEGACY)
public class WebappDisclosureControllerTest {
    private static final String UNBOUND_PACKAGE = "unbound";
    private static final String BOUND_PACKAGE = WebApkConstants.WEBAPK_PACKAGE_PREFIX + ".bound";
    private static final String SCOPE = "https://www.example.com";

    @Mock public CurrentPageVerifier mCurrentPageVerifier;

    @Captor public ArgumentCaptor<Runnable> mVerificationObserverCaptor;

    public TrustedWebActivityModel mModel = new TrustedWebActivityModel();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        // Run AsyncTasks synchronously.
        PostTask.setPrenativeThreadPoolExecutorForTesting(new RoboExecutorService());

        doNothing()
                .when(mCurrentPageVerifier)
                .addVerificationObserver(mVerificationObserverCaptor.capture());
    }

    private WebappDisclosureController buildControllerForWebApk(String webApkPackageName) {
        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(webApkPackageName, "https://pwa.rocks/")
                        .build();
        return new WebappDisclosureController(
                intentDataProvider,
                mock(WebappDeferredStartupWithStorageHandler.class),
                mModel,
                mock(ActivityLifecycleDispatcher.class),
                mCurrentPageVerifier);
    }

    private WebappDataStorage registerStorageForWebApk(String packageName) {
        String id = WebappIntentUtils.getIdForWebApkPackage(packageName);
        WebappRegistry.getInstance().register(id, (storage) -> {});
        return WebappRegistry.getInstance().getWebappDataStorage(id);
    }

    public void verifyShownThenDismissedOnNewCreateStorage(String packageName) {
        WebappDisclosureController controller = buildControllerForWebApk(packageName);
        WebappDataStorage storage = registerStorageForWebApk(packageName);
        setVerificationStatus(VerificationStatus.SUCCESS);

        // Simulates the case that shows the disclosure when creating a new storage.
        controller.onDeferredStartupWithStorage(storage, /* didCreateStorage= */ true);
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
        setVerificationStatus(VerificationStatus.SUCCESS);

        // Simulates the case that shows the disclosure when finish native initialization.
        storage.setShowDisclosure();
        assertTrue(storage.shouldShowDisclosure());
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
        controller.onDeferredStartupWithStorage(storage, /* didCreateStorage= */ false);
        assertSnackbarNotShown();

        storage.delete();
    }

    public void verifyNeverShown(String packageName) {
        WebappDisclosureController controller = buildControllerForWebApk(packageName);
        WebappDataStorage storage = registerStorageForWebApk(packageName);

        // Try to show the disclosure the first time.
        controller.onDeferredStartupWithStorage(storage, /* didCreateStorage= */ true);
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

    private void setVerificationStatus(@VerificationStatus int status) {
        VerificationState state = new VerificationState(SCOPE, SCOPE, status);
        doReturn(state).when(mCurrentPageVerifier).getState();

        for (Runnable observer : mVerificationObserverCaptor.getAllValues()) {
            observer.run();
        }
    }

    @Test
    @Feature({"Webapps"})
    public void testUnboundWebApkShowDisclosure() {
        verifyShownThenDismissedOnNewCreateStorage(UNBOUND_PACKAGE);
    }

    @Test
    @Feature({"Webapps"})
    public void testUnboundWebApkShowDisclosure2() {
        verifyShownThenDismissedOnRestart(UNBOUND_PACKAGE);
    }

    @Test
    @Feature({"Webapps"})
    public void testUnboundWebApkNoDisclosureOnExistingStorage() {
        verifyNotShownOnExistingStorageWithoutShouldShowDisclosure(UNBOUND_PACKAGE);
    }

    @Test
    @Feature({"Webapps"})
    public void testBoundWebApkNoDisclosure() {
        verifyNeverShown(BOUND_PACKAGE);
    }

    @Test
    @Feature({"Webapps"})
    public void testNotShowDisclosureWhenNotVerifiedOrigin() {
        WebappDisclosureController controller = buildControllerForWebApk(UNBOUND_PACKAGE);
        WebappDataStorage storage = registerStorageForWebApk(UNBOUND_PACKAGE);

        setVerificationStatus(VerificationStatus.FAILURE);
        controller.onDeferredStartupWithStorage(storage, /* didCreateStorage= */ true);
        assertTrue(storage.shouldShowDisclosure());

        assertSnackbarNotShown();
    }

    @Test
    @Feature({"Webapps"})
    public void testDismissDisclosureWhenLeavingVerifiedOrigin() {
        WebappDisclosureController controller = buildControllerForWebApk(UNBOUND_PACKAGE);
        WebappDataStorage storage = registerStorageForWebApk(UNBOUND_PACKAGE);
        storage.setShowDisclosure();
        controller.onFinishNativeInitialization();

        setVerificationStatus(VerificationStatus.SUCCESS);
        assertSnackbarShown();

        setVerificationStatus(VerificationStatus.FAILURE);
        assertSnackbarNotShown();
    }

    @Test
    @Feature({"Webapps"})
    public void testShowsAgainWhenReenteringTrustedOrigin() {
        WebappDisclosureController controller = buildControllerForWebApk(UNBOUND_PACKAGE);
        WebappDataStorage storage = registerStorageForWebApk(UNBOUND_PACKAGE);
        storage.setShowDisclosure();
        controller.onFinishNativeInitialization();

        setVerificationStatus(VerificationStatus.SUCCESS);
        setVerificationStatus(VerificationStatus.FAILURE);
        setVerificationStatus(VerificationStatus.SUCCESS);
        assertSnackbarShown();
    }
}
