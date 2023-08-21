// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.os.Build;
import android.view.Window;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.ui.util.ColorUtils;

/** Tests for {@link CustomTabNavigationBarController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class CustomTabNavigationBarControllerTest {
    @Mock
    private ColorProvider mColorProvider;
    @Mock
    private CustomTabIntentDataProvider mCustomTabIntentDataProvider;
    private Window mWindow;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        mWindow = spy(activity.getWindow());
        mContext = activity;
        when(mCustomTabIntentDataProvider.getColorProvider()).thenReturn(mColorProvider);
    }

    @Test
    public void doesNotSetBarColorWhenNull() {
        when(mColorProvider.getNavigationBarColor()).thenReturn(null);
        CustomTabNavigationBarController.update(mWindow, mCustomTabIntentDataProvider, mContext);

        verify(mWindow, never()).setNavigationBarColor(Mockito.anyInt());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P) // Android P+ (>=28) is needed for setting divider color.
    public void doesNotSetDividerColorWhenNull() {
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(null);
        // Bar color needs to be null. Otherwise the divider color could still be set if
        // needsDarkButtons is true.
        when(mColorProvider.getNavigationBarColor()).thenReturn(null);

        CustomTabNavigationBarController.update(mWindow, mCustomTabIntentDataProvider, mContext);
        verify(mWindow, never()).setNavigationBarDividerColor(Mockito.anyInt());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.O_MR1)
    // Android P+ (>=28) is needed for setting the divider color.
    public void doesNotSetDividerColorWhenSdkLow() {
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(Color.RED);
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.GREEN);

        // Make sure calling the line below does not throw an exception, because the method does not
        // exist in android P+.
        CustomTabNavigationBarController.update(mWindow, mCustomTabIntentDataProvider, mContext);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1) // SDK 25 is used to trigger supportsDarkButtons=false.
    public void setsCorrectBarColor() {
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(Color.RED);

        // The case when needsDarkButtons=true.
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.WHITE);
        CustomTabNavigationBarController.update(mWindow, mCustomTabIntentDataProvider, mContext);
        verify(mWindow).setNavigationBarColor(ColorUtils.getDarkenedColorForStatusBar(Color.WHITE));

        // The case when needsDarkButtons=false.
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.BLACK);
        CustomTabNavigationBarController.update(mWindow, mCustomTabIntentDataProvider, mContext);
        verify(mWindow).setNavigationBarColor(Color.BLACK);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.O) // SDK 26 is used to trigger supportDarkButtons=true.
    public void setsCorrectBarColorWhenDarkButtonsSupported() {
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(Color.RED);
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.GREEN);

        // The case when needsDarkButtons=true
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.WHITE);
        CustomTabNavigationBarController.update(mWindow, mCustomTabIntentDataProvider, mContext);
        verify(mWindow).setNavigationBarColor(Color.WHITE);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P) // Android P+ (>=28) needed for setting divider color.
    public void setsCorrectDividerColor() {
        // The case when divider color is set explicitly.
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(Color.RED);
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.BLACK);

        CustomTabNavigationBarController.update(mWindow, mCustomTabIntentDataProvider, mContext);
        verify(mWindow).setNavigationBarDividerColor(Color.RED);

        // The case when divider color is set implicitly due to needsDarkButtons=true.
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(null);
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.WHITE);
        CustomTabNavigationBarController.update(mWindow, mCustomTabIntentDataProvider, mContext);
        verify(mWindow).setNavigationBarDividerColor(
                mContext.getColor(org.chromium.chrome.R.color.black_alpha_12));
    }
}
