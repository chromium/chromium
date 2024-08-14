// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import android.app.Activity;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mockito;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.BottomContainer;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Unit tests for {@link BadFlagsSnackbarManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BadFlagsSnackbarManagerTest {
    Activity mActivity;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testCreateSnackbar() {
        String errorString = "Unsupported flag.";
        ViewGroup viewGroup = new BottomContainer(mActivity, null);
        SnackbarManager snackbarManager =
                Mockito.spy(new SnackbarManager(mActivity, viewGroup, null));
        BadFlagsSnackbarManager.createSnackbar(errorString, snackbarManager);
        Snackbar snackbar = snackbarManager.getCurrentSnackbarForTesting();
        Mockito.verify(snackbarManager).showSnackbar(ArgumentMatchers.any());
        Assert.assertNull("Snackbar controller should be null.", snackbar.getController());
        Assert.assertEquals(
                "Snackbar text should match.", errorString, snackbar.getTextForTesting());
        Assert.assertEquals(
                "Snackbar identifier should match.",
                Snackbar.UMA_BAD_FLAGS,
                snackbar.getIdentifierForTesting());
        Assert.assertEquals(
                "Snackbar dismiss duration is incorrect.",
                SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS,
                snackbar.getDuration());
        snackbarManager.dismissSnackbars(null);
    }

    @Test
    public void testCreateSnackbar_NoCrashOnNullSnackbarManager() {
        String errorString = "Unsupported flag.";
        BadFlagsSnackbarManager.createSnackbar(errorString, null);
    }
}
