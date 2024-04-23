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

/** Test for Tracking Protection Snackbar controller for WebApk. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrackingProtectionSnackbarControllerTest {

    @Mock private Supplier<SnackbarManager> mSnackbarManagerSupplierMock;

    @Mock private SnackbarManager mSnackbarManagerMock;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
    public void testShowSnackbar() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();
        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(null, mSnackbarManagerSupplierMock);

        controller.showSnackbar(ActivityType.WEB_APK);
        verify(mSnackbarManagerMock, times(1)).showSnackbar(any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
    public void testShowSnackbarForNonWebApk() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();
        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(null, mSnackbarManagerSupplierMock);

        controller.showSnackbar(ActivityType.TRUSTED_WEB_ACTIVITY);
        controller.showSnackbar(ActivityType.CUSTOM_TAB);
        controller.showSnackbar(ActivityType.PRE_FIRST_TAB);
        controller.showSnackbar(ActivityType.TABBED);
        controller.showSnackbar(ActivityType.WEBAPP);

        verifyNoInteractions(mSnackbarManagerMock);
        verifyNoInteractions(mSnackbarManagerSupplierMock);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
    public void testShowSnackbarForDisabledFeature() {
        doNothing().when(mSnackbarManagerMock).showSnackbar(any());
        doReturn(mSnackbarManagerMock).when(mSnackbarManagerSupplierMock).get();
        TrackingProtectionSnackbarController controller =
                new TrackingProtectionSnackbarController(null, mSnackbarManagerSupplierMock);

        controller.showSnackbar(ActivityType.WEB_APK);

        verifyNoInteractions(mSnackbarManagerMock);
        verifyNoInteractions(mSnackbarManagerSupplierMock);
    }
}
