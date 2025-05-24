// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.content_settings.CookieControlsBridgeJni;
import org.chromium.components.content_settings.CookieControlsState;

/** Test for Tracking Protection Snackbar controller for WebApk. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrackingProtectionSnackbarControllerTest {

    @Mock private Supplier<SnackbarManager> mSnackbarManagerSupplierMock;

    @Mock private SnackbarManager mSnackbarManagerMock;

    private FakeCookieControlsBridge mFakeCookieControlsBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mFakeCookieControlsBridge = new FakeCookieControlsBridge();
        CookieControlsBridgeJni.setInstanceForTesting(mFakeCookieControlsBridge);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
    @Features.DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER)
    public void testShowSnackbar() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();
        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(
                        null,
                        mSnackbarManagerSupplierMock,
                        null,
                        null,
                        ActivityType.WEB_APK,
                        false);

        controller.onStatusChanged(CookieControlsState.ALLOWED3PC, 0, 0, 0);
        controller.maybeTriggerSnackbar();
        verify(mSnackbarManagerMock, times(1)).showSnackbar(any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
    @Features.DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER)
    public void testShowSnackbarTriggeredByReloadEvent() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();
        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(
                        null,
                        mSnackbarManagerSupplierMock,
                        null,
                        null,
                        ActivityType.WEB_APK,
                        false);

        controller.onStatusChanged(CookieControlsState.ALLOWED3PC, 0, 0, 0);
        controller.onHighlightPwaCookieControl();
        verify(mSnackbarManagerMock, times(1)).showSnackbar(any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
    @Features.DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER)
    public void testShowSnackbarTriggeredByMultipleReloadEvents() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();
        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(
                        null,
                        mSnackbarManagerSupplierMock,
                        null,
                        null,
                        ActivityType.WEB_APK,
                        false);

        controller.onStatusChanged(CookieControlsState.ALLOWED3PC, 0, 0, 0);
        controller.onHighlightPwaCookieControl();
        verify(mSnackbarManagerMock, times(1)).showSnackbar(any());

        controller.onHighlightPwaCookieControl();
        verifyNoMoreInteractions(mSnackbarManagerMock);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
    @Features.DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER)
    public void testShowSnackbarTriggeredByReloadEventsWithoutTrackingProtection() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();

        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(
                        null,
                        mSnackbarManagerSupplierMock,
                        null,
                        null,
                        ActivityType.WEB_APK,
                        false);

        controller.onStatusChanged(CookieControlsState.HIDDEN, 0, 0, 0);
        controller.onHighlightPwaCookieControl();
        verifyNoInteractions(mSnackbarManagerMock);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
    @Features.DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER)
    public void testShowSnackbarTriggeredByReloadEventsWithProtectionsOn() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();
        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(
                        null,
                        mSnackbarManagerSupplierMock,
                        null,
                        null,
                        ActivityType.WEB_APK,
                        false);

        controller.onStatusChanged(CookieControlsState.HIDDEN, 0, 0, 0);
        controller.onHighlightPwaCookieControl();
        verifyNoInteractions(mSnackbarManagerMock);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
    @Features.DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER)
    public void testShowSnackbarForNonWebApk() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();

        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(
                        null,
                        mSnackbarManagerSupplierMock,
                        null,
                        null,
                        ActivityType.TRUSTED_WEB_ACTIVITY,
                        false);
        controller.onStatusChanged(CookieControlsState.ALLOWED3PC, 0, 0, 0);
        controller.maybeTriggerSnackbar();
        controller =
                new TrackingProtectionSnackbarController(
                        null,
                        mSnackbarManagerSupplierMock,
                        null,
                        null,
                        ActivityType.CUSTOM_TAB,
                        false);
        controller.onStatusChanged(CookieControlsState.ALLOWED3PC, 0, 0, 0);
        controller.maybeTriggerSnackbar();
        controller =
                new TrackingProtectionSnackbarController(
                        null,
                        mSnackbarManagerSupplierMock,
                        null,
                        null,
                        ActivityType.PRE_FIRST_TAB,
                        false);
        controller.onStatusChanged(CookieControlsState.ALLOWED3PC, 0, 0, 0);
        controller.maybeTriggerSnackbar();
        controller =
                new TrackingProtectionSnackbarController(
                        null, mSnackbarManagerSupplierMock, null, null, ActivityType.TABBED, false);
        controller.onStatusChanged(CookieControlsState.ALLOWED3PC, 0, 0, 0);
        controller.maybeTriggerSnackbar();
        controller =
                new TrackingProtectionSnackbarController(
                        null, mSnackbarManagerSupplierMock, null, null, ActivityType.WEBAPP, false);
        controller.onStatusChanged(CookieControlsState.ALLOWED3PC, 0, 0, 0);
        controller.maybeTriggerSnackbar();

        verifyNoInteractions(mSnackbarManagerMock);
        verifyNoInteractions(mSnackbarManagerSupplierMock);
    }

    @Test
    @Features.DisableFeatures({
        ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA,
        ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER
    })
    public void testShowSnackbarForDisabledFeature() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();
        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(
                        null,
                        mSnackbarManagerSupplierMock,
                        null,
                        null,
                        ActivityType.WEB_APK,
                        false);

        controller.maybeTriggerSnackbar();

        verifyNoInteractions(mSnackbarManagerMock);
        verifyNoInteractions(mSnackbarManagerSupplierMock);
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA,
        ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER
    })
    public void testForceShowSnackbar() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();
        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(
                        null,
                        mSnackbarManagerSupplierMock,
                        null,
                        null,
                        ActivityType.WEB_APK,
                        false);

        controller.maybeTriggerSnackbar();
        verify(mSnackbarManagerMock, times(1)).showSnackbar(any());
    }
}
