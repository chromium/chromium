// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
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
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/**
 * Robolectric tests for {@link IncognitoCustomTabSnapshotController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class IncognitoCustomTabSnapshotControllerTest {
    @Mock
    private Window mWindowMock;

    @Mock
    private Activity mActivityMock;

    private boolean mIsIncognitoShowing;
    private WindowManager.LayoutParams mParams;
    private final Supplier<Boolean> mIsIncognitoShowingSupplier = () -> mIsIncognitoShowing;

    @Rule
    public TestRule mJunitProcessor = new Features.JUnitProcessor();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mParams = new WindowManager.LayoutParams();
        doReturn(mParams).when(mWindowMock).getAttributes();
        doReturn(mWindowMock).when(mActivityMock).getWindow();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT,
            ChromeFeatureList.IMPROVED_INCOGNITO_SCREENSHOT})
    public void
    testSecureFlagsAdded() {
        mParams.flags = 0;
        mIsIncognitoShowing = true;
        new IncognitoCustomTabSnapshotController(mActivityMock, mIsIncognitoShowingSupplier);

        verify(mWindowMock, times(1)).addFlags(WindowManager.LayoutParams.FLAG_SECURE);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    @DisableFeatures({ChromeFeatureList.IMPROVED_INCOGNITO_SCREENSHOT})
    public void testSecureFlagsRemoved() {
        mParams.flags = WindowManager.LayoutParams.FLAG_SECURE;
        mIsIncognitoShowing = true;
        new IncognitoCustomTabSnapshotController(mActivityMock, mIsIncognitoShowingSupplier);

        verify(mWindowMock, times(1)).clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.IMPROVED_INCOGNITO_SCREENSHOT})
    @Config(minSdk = Build.VERSION_CODES.TIRAMISU)
    public void testRecentsScreenshotsEnabled_ForAndroidTOrAbove_AfterSwitchingToNonIncognito() {
        mIsIncognitoShowing = false;
        new IncognitoCustomTabSnapshotController(mActivityMock, mIsIncognitoShowingSupplier);

        verify(mActivityMock, times(1)).setRecentsScreenshotEnabled(true);
        assertEquals(0, mParams.flags);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.IMPROVED_INCOGNITO_SCREENSHOT})
    @Config(minSdk = Build.VERSION_CODES.TIRAMISU)
    public void testRecentsScreenshotsDisabled_ForAndroidTOrAbove_AfterSwitchingToIncognito() {
        mIsIncognitoShowing = true;
        new IncognitoCustomTabSnapshotController(mActivityMock, mIsIncognitoShowingSupplier);

        verify(mActivityMock, times(1)).setRecentsScreenshotEnabled(false);
        assertEquals(0, mParams.flags);
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT,
            ChromeFeatureList.IMPROVED_INCOGNITO_SCREENSHOT})
    @Config(minSdk = Build.VERSION_CODES.TIRAMISU)
    public void
    testSecureFlagsAdded_ForAndroidTOrAbove_WhenImprovedIncognitoScreenshotDisabled() {
        mParams.flags = 0;
        mIsIncognitoShowing = true;
        new IncognitoCustomTabSnapshotController(mActivityMock, mIsIncognitoShowingSupplier);

        verify(mActivityMock, never()).setRecentsScreenshotEnabled(false);
        verify(mWindowMock, times(1)).addFlags(WindowManager.LayoutParams.FLAG_SECURE);
    }
}
