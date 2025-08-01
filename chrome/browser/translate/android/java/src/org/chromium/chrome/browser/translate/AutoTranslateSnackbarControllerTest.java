// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link AutoTranslateSnackbarController} */
@RunWith(BaseRobolectricTestRunner.class)
public final class AutoTranslateSnackbarControllerTest {
    private static final int NATIVE_SNACKBAR_VIEW = 1001;
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock AutoTranslateSnackbarController.Natives mMockJni;

    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        AutoTranslateSnackbarControllerJni.setInstanceForTesting(mMockJni);
    }

    @Test
    @SmallTest
    public void testCreateWithNullWindowAndroid() {
        Mockito.doReturn(null).when(mWebContents).getTopLevelNativeWindow();
        Assert.assertNull(
                AutoTranslateSnackbarController.create(mWebContents, NATIVE_SNACKBAR_VIEW));
    }

    @Test
    @SmallTest
    public void testCreateWithNullActivityWeakReference() {
        Mockito.doReturn(mWindowAndroid).when(mWebContents).getTopLevelNativeWindow();
        Mockito.doReturn(null).when(mWindowAndroid).getActivity();
        Assert.assertNull(
                AutoTranslateSnackbarController.create(mWebContents, NATIVE_SNACKBAR_VIEW));
    }

    @Test
    @SmallTest
    public void testCreateWithNullSnackbarManager() {
        Activity activity = Mockito.mock(Activity.class);

        Mockito.doReturn(new WeakReference<>(activity)).when(mWindowAndroid).getActivity();
        Mockito.doReturn(mWindowAndroid).when(mWebContents).getTopLevelNativeWindow();
        Mockito.doReturn(new UnownedUserDataHost()).when(mWindowAndroid).getUnownedUserDataHost();
        Assert.assertNull(
                AutoTranslateSnackbarController.create(mWebContents, NATIVE_SNACKBAR_VIEW));
    }
}
