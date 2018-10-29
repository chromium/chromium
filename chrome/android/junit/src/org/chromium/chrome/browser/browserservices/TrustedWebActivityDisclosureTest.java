// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;

/**
 * Tests for {@link TrustedWebActivityDisclosure}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityDisclosureTest {
    private static final String TWA_PACKAGE = "com.example.twa";

    @Mock public Resources mResources;
    @Mock public ChromePreferenceManager mPreferences;
    @Mock public SnackbarManager mSnackbarManager;

    private TrustedWebActivityDisclosure mDisclosure;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn("any string").when(mResources).getString(anyInt());
        doReturn(false).when(mPreferences).hasUserAcceptedTwaDisclosureForPackage(anyString());

        mDisclosure = new TrustedWebActivityDisclosure(mResources, mPreferences,
                () -> mSnackbarManager);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void showIsIdempotent() {
        mDisclosure.showIfNeeded(TWA_PACKAGE);
        mDisclosure.showIfNeeded(TWA_PACKAGE);

        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @Feature("TrustedWebActivities")
    public void hideIsIdempotent() {
        mDisclosure.showIfNeeded(TWA_PACKAGE);

        mDisclosure.dismiss();
        mDisclosure.dismiss();

        verify(mSnackbarManager).dismissSnackbars(any());
    }

    @Test
    @Feature("TrustedWebActivities")
    public void noShowIfAlreadyAccepted() {
        doReturn(true).when(mPreferences).hasUserAcceptedTwaDisclosureForPackage(anyString());

        mDisclosure.showIfNeeded(TWA_PACKAGE);

        verify(mSnackbarManager, times(0)).showSnackbar(any());
    }

    @Test
    @Feature("TrustedWebActivities")
    public void recordDismiss() {
        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        mDisclosure.showIfNeeded(TWA_PACKAGE);

        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());

        // Dismiss the Snackbar.
        // TODO(peconn): Refactor Snackbar to make this a bit cleaner.
        snackbarCaptor.getValue().getController().onAction(
                snackbarCaptor.getValue().getActionDataForTesting());

        verify(mPreferences).setUserAcceptedTwaDisclosureForPackage(TWA_PACKAGE);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doNothingOnNoSnackbarAction() {
        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        mDisclosure.showIfNeeded(TWA_PACKAGE);

        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());

        // Dismiss the Snackbar.
        // TODO(peconn): Refactor Snackbar to make this a bit cleaner.
        snackbarCaptor.getValue().getController().onDismissNoAction(
                snackbarCaptor.getValue().getActionDataForTesting());

        verify(mPreferences, times(0)).setUserAcceptedTwaDisclosureForPackage(anyString());
    }
}
