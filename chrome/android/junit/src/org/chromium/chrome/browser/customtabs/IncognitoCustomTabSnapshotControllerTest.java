// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Robolectric tests for {@link IncognitoCustomTabSnapshotController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IncognitoCustomTabSnapshotControllerTest {
    @Mock
    private Window mWindowMock;

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
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlagsAdded() {
        mParams.flags = 0;
        mIsIncognitoShowing = true;
        new IncognitoCustomTabSnapshotController(mWindowMock, mIsIncognitoShowingSupplier);

        verify(mWindowMock, times(1)).addFlags(WindowManager.LayoutParams.FLAG_SECURE);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlagsRemoved() {
        mParams.flags = WindowManager.LayoutParams.FLAG_SECURE;
        mIsIncognitoShowing = true;
        new IncognitoCustomTabSnapshotController(mWindowMock, mIsIncognitoShowingSupplier);

        verify(mWindowMock, times(1)).clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
    }
}
