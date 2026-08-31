// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_FIRST_TIME;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_SCOPE;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_DISMISSED_BY_USER;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browserservices.BrowserServicesStore;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationState;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Tests for {@link TrustedWebActivityDisclosureController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp")
@EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_TWA_DISCLOSURES)
public class TrustedWebActivityDisclosureControllerTest {
    private static final String CLIENT_PACKAGE = "com.example.twaclient";
    private static final String SCOPE = "https://www.example.com";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock public CurrentPageVerifier mCurrentPageVerifier;
    @Mock public ClientPackageNameProvider mClientPackageNameProvider;
    @Mock public WindowAndroid mWindowAndroid;

    @Captor public ArgumentCaptor<Runnable> mVerificationObserverCaptor;

    public TrustedWebActivityModel mModel = new TrustedWebActivityModel();
    private TrustedWebActivityDisclosureController mController;

    @Before
    public void setUp() {
        WeakReference<Context> weakContext =
                new WeakReference<>(ContextUtils.getApplicationContext());
        doReturn(weakContext).when(mWindowAndroid).getContext();

        doReturn(CLIENT_PACKAGE).when(mClientPackageNameProvider).get();
        doNothing()
                .when(mCurrentPageVerifier)
                .addVerificationObserver(mVerificationObserverCaptor.capture());

        mController =
                new TrustedWebActivityDisclosureController(
                        mWindowAndroid,
                        mModel,
                        mLifecycleDispatcher,
                        mCurrentPageVerifier,
                        mClientPackageNameProvider);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void noShowWhenOriginVerified() {
        ensureOriginVerificationSuccess();
        assertSnackbarNotShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void showWhenOriginVerificationFailed() {
        ensureOriginVerificationFailed();
        assertSnackbarShown();
        assertScope(SCOPE);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void showsWhenLeavingVerifiedOrigin() {
        ensureOriginVerificationSuccess();
        assertSnackbarNotShown();
        ensureOriginVerificationFailed();
        assertSnackbarShown();
        assertScope(SCOPE);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void pendingOriginVerifiedNoShow() {
        enterOriginVerificationPending();
        assertSnackbarNotShown();
        ensureOriginVerificationSuccess();
        assertSnackbarNotShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void pendingOriginNotVerifiedShows() {
        enterOriginVerificationPending();
        ensureOriginVerificationFailed();
        assertSnackbarShown();
        assertScope(SCOPE);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void dismissesWhenReenteringTrustedOrigin() {
        ensureOriginVerificationSuccess();
        ensureOriginVerificationFailed();
        assertSnackbarShown();
        assertScope(SCOPE);
        ensureOriginVerificationSuccess();
        assertSnackbarNotShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void noShowIfAlreadyAccepted() {
        BrowserServicesStore.setUserAcceptedTwaDisclosureForPackage(CLIENT_PACKAGE);
        ensureOriginVerificationFailed();
        assertSnackbarNotShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void recordDismissAfterShown() {
        ensureOriginVerificationFailed();
        assertSnackbarShown();
        dismissSnackbar();
        assertTrue(BrowserServicesStore.hasUserAcceptedTwaDisclosureForPackage(CLIENT_PACKAGE));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void reportsFirstTime_firstTime() {
        ensureOriginVerificationFailed();
        assertTrue(mModel.get(DISCLOSURE_FIRST_TIME));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void reportsFirstTime_notFirstTime() {
        BrowserServicesStore.setUserSeenTwaDisclosureForPackage(CLIENT_PACKAGE);
        ensureOriginVerificationFailed();
        assertFalse(mModel.get(DISCLOSURE_FIRST_TIME));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void reportsFirstTime_reportsSeenImmediately() {
        ensureOriginVerificationFailed();
        assertTrue(mModel.get(DISCLOSURE_FIRST_TIME));
        mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureShown();
        assertFalse(mModel.get(DISCLOSURE_FIRST_TIME));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void recordsShown() {
        ensureOriginVerificationFailed();
        mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureShown();
        assertTrue(BrowserServicesStore.hasUserSeenTwaDisclosureForPackage(CLIENT_PACKAGE));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void noticesShouldShowDisclosureChanges() {
        mController.onFinishNativeInitialization();
        ensureOriginVerificationFailed();
        assertSnackbarShown();

        BrowserServicesStore.setUserAcceptedTwaDisclosureForPackage(CLIENT_PACKAGE);
        mController.onStopWithNative();

        assertEquals(DISCLOSURE_STATE_DISMISSED_BY_USER, mModel.get(DISCLOSURE_STATE));
    }

    private void enterOriginVerificationPending() {
        setVerificationState(new VerificationState(SCOPE, SCOPE, VerificationStatus.PENDING));
    }

    private void ensureOriginVerificationSuccess() {
        setVerificationState(new VerificationState(SCOPE, SCOPE, VerificationStatus.SUCCESS));
    }

    private void ensureOriginVerificationFailed() {
        setVerificationState(new VerificationState(SCOPE, SCOPE, VerificationStatus.FAILURE));
    }

    @Test
    @Feature("TrustedWebActivities")
    @DisableFeatures(ChromeFeatureList.DESKTOP_ANDROID_TWA_DISCLOSURES)
    public void oldBehavior_noShowWhenOriginVerificationFailed() {
        ensureOriginVerificationFailed();
        assertSnackbarNotShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    @DisableFeatures(ChromeFeatureList.DESKTOP_ANDROID_TWA_DISCLOSURES)
    public void oldBehavior_showWhenOriginVerified() {
        ensureOriginVerificationSuccess();
        assertSnackbarShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    @Config(qualifiers = "sw320dp")
    public void mobileBehavior_noShowWhenOriginVerificationFailed() {
        ensureOriginVerificationFailed();
        assertSnackbarNotShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    @Config(qualifiers = "sw320dp")
    public void mobileBehavior_showWhenOriginVerified() {
        ensureOriginVerificationSuccess();
        assertSnackbarShown();
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
