// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.Build;
import android.view.Window;
import android.view.WindowManager;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Robolectric tests for {@link IncognitoCustomTabSnapshotController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class IncognitoCustomTabSnapshotControllerTest {

    @Mock private Window mWindowMock;

    @Mock private Activity mActivityMock;

    private boolean mIsIncognitoShowing;
    private WindowManager.LayoutParams mParams;
    private final Supplier<Boolean> mIsIncognitoShowingSupplier = () -> mIsIncognitoShowing;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mParams = new WindowManager.LayoutParams();
        doReturn(mParams).when(mWindowMock).getAttributes();
        doReturn(mWindowMock).when(mActivityMock).getWindow();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlagsAdded() {
        mParams.flags = 0;
        mIsIncognitoShowing = true;
        new IncognitoCustomTabSnapshotController(mActivityMock, mIsIncognitoShowingSupplier);

        verify(mWindowMock, times(1)).addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            verify(mActivityMock, never()).setRecentsScreenshotEnabled(anyBoolean());
        }
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlagsRemoved() {
        mParams.flags = WindowManager.LayoutParams.FLAG_SECURE;
        mIsIncognitoShowing = true;
        new IncognitoCustomTabSnapshotController(mActivityMock, mIsIncognitoShowingSupplier);

        verify(mWindowMock, times(1)).clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            verify(mActivityMock, times(1)).setRecentsScreenshotEnabled(false);
        }
    }
}
