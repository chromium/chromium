// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_FIRST_TIME;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_SCOPE;
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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.BrowserServicesStore;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationState;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

/** Tests for {@link TrustedWebActivityDisclosureController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityDisclosureControllerTest {
    private static final String CLIENT_PACKAGE = "com.example.twaclient";
    private static final String SCOPE = "https://www.example.com";

    @Mock public BrowserServicesStore mStore;
    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock public CurrentPageVerifier mCurrentPageVerifier;
    @Mock public TrustedWebActivityUmaRecorder mRecorder;
    @Mock public ClientPackageNameProvider mClientPackageNameProvider;

    @Captor public ArgumentCaptor<Runnable> mVerificationObserverCaptor;

    public TrustedWebActivityModel mModel = new TrustedWebActivityModel();
    private TrustedWebActivityDisclosureController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(CLIENT_PACKAGE).when(mClientPackageNameProvider).get();
        doNothing()
                .when(mCurrentPageVerifier)
                .addVerificationObserver(mVerificationObserverCaptor.capture());
        doReturn(false).when(mStore).hasUserAcceptedTwaDisclosureForPackage(anyString());

        mController =
                new TrustedWebActivityDisclosureController(
                        mStore,
                        mModel,
                        mLifecycleDispatcher,
                        mCurrentPageVerifier,
                        mRecorder,
                        mClientPackageNameProvider);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void showsWhenOriginVerified() {
        enterVerifiedOrigin();
        assertSnackbarShown();
        assertScope(SCOPE);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void dismissesWhenLeavingVerifiedOrigin() {
        enterVerifiedOrigin();
        exitVerifiedOrigin();
        assertSnackbarNotShown();
        assertScope(null);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void showsAgainWhenReenteringTrustedOrigin() {
        enterVerifiedOrigin();
        exitVerifiedOrigin();
        enterVerifiedOrigin();
        assertSnackbarShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void noShowIfAlreadyAccepted() {
        doReturn(true).when(mStore).hasUserAcceptedTwaDisclosureForPackage(anyString());
        enterVerifiedOrigin();
        assertSnackbarNotShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void recordDismiss() {
        enterVerifiedOrigin();
        dismissSnackbar();
        verify(mStore).setUserAcceptedTwaDisclosureForPackage(CLIENT_PACKAGE);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void reportsFirstTime_firstTime() {
        doReturn(false).when(mStore).hasUserSeenTwaDisclosureForPackage(anyString());
        enterVerifiedOrigin();
        assertTrue(mModel.get(DISCLOSURE_FIRST_TIME));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void reportsFirstTime_notFirstTime() {
        doReturn(true).when(mStore).hasUserSeenTwaDisclosureForPackage(anyString());
        enterVerifiedOrigin();
        assertFalse(mModel.get(DISCLOSURE_FIRST_TIME));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void reportsFirstTime_reportsSeenImmediately() {
        doReturn(false).when(mStore).hasUserSeenTwaDisclosureForPackage(anyString());
        enterVerifiedOrigin();
        assertTrue(mModel.get(DISCLOSURE_FIRST_TIME));
        mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureShown();
        assertFalse(mModel.get(DISCLOSURE_FIRST_TIME));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void recordsShown() {
        enterVerifiedOrigin();
        mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureShown();
        verify(mStore).setUserSeenTwaDisclosureForPackage(CLIENT_PACKAGE);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void noticesShouldShowDisclosureChanges() {
        mController.onFinishNativeInitialization();
        enterVerifiedOrigin();
        assertSnackbarShown();

        doReturn(true).when(mStore).hasUserAcceptedTwaDisclosureForPackage(anyString());
        mController.onStopWithNative();

        assertEquals(DISCLOSURE_STATE_DISMISSED_BY_USER, mModel.get(DISCLOSURE_STATE));
    }

    private void enterVerifiedOrigin() {
        setVerificationState(new VerificationState(SCOPE, SCOPE, VerificationStatus.SUCCESS));
    }

    private void exitVerifiedOrigin() {
        setVerificationState(new VerificationState(SCOPE, SCOPE, VerificationStatus.FAILURE));
    }

    private void setVerificationState(VerificationState state) {
        doReturn(state).when(mCurrentPageVerifier).getState();
        for (Runnable observer : mVerificationObserverCaptor.getAllValues()) {
            observer.run();
        }
    }

    private void assertSnackbarShown() {
        assertEquals(DISCLOSURE_STATE_SHOWN, mModel.get(DISCLOSURE_STATE));
    }

    private void assertSnackbarNotShown() {
        assertEquals(DISCLOSURE_STATE_NOT_SHOWN, mModel.get(DISCLOSURE_STATE));
    }

    private void assertScope(String scope) {
        assertEquals(scope, mModel.get(DISCLOSURE_SCOPE));
    }

    private void dismissSnackbar() {
        mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureAccepted();
    }
}
