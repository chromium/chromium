// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

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
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.ui.BottomContainer;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.base.TestActivity;

/**
 * Unit tests for ChromeActivity.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeActivityUnitTest {
    Activity mActivity;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
    }

    @Test
    public void testCreateWindowErrorSnackbar() {
        String errorString = "Some error.";
        ViewGroup viewGroup = new BottomContainer(mActivity, null);
        SnackbarManager snackbarManager =
                Mockito.spy(new SnackbarManager(mActivity, viewGroup, null));
        ChromeActivity.createWindowErrorSnackbar(errorString, snackbarManager);
        Snackbar snackbar = snackbarManager.getCurrentSnackbarForTesting();
        Mockito.verify(snackbarManager).showSnackbar(ArgumentMatchers.any());
        Assert.assertNull("Snackbar controller should be null.", snackbar.getController());
        Assert.assertEquals(
                "Snackbar text should match.", errorString, snackbar.getTextForTesting());
        Assert.assertEquals("Snackbar identifier should match.", Snackbar.UMA_WINDOW_ERROR,
                snackbar.getIdentifierForTesting());
        Assert.assertEquals("Snackbar dismiss duration is incorrect.",
                SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS, snackbar.getDuration());
        snackbarManager.dismissSnackbars(null);
    }
}
