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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Tests for {@link CustomTabNavigationBarController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class CustomTabNavigationBarControllerTest {
    @Mock private ColorProvider mColorProvider;
    @Mock private CustomTabIntentDataProvider mCustomTabIntentDataProvider;
    @Mock private CustomTabsConnection mConnection;
    private Window mWindow;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        mWindow = spy(activity.getWindow());
        mContext = activity;
        CustomTabsConnection.setInstanceForTesting(mConnection);
        when(mCustomTabIntentDataProvider.getColorProvider()).thenReturn(mColorProvider);
    }

    @Test
    public void doesNotSetBarColorWhenNull() {
        when(mColorProvider.getNavigationBarColor()).thenReturn(null);
        CustomTabNavigationBarController.update(
                mWindow, mCustomTabIntentDataProvider, mContext, /* isEdgeToEdge= */ false);

        verify(mWindow, never()).setNavigationBarColor(Mockito.anyInt());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P) // Android P+ (>=28) is needed for setting divider color.
    public void doesNotSetDividerColorWhenNull() {
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(null);
        // Bar color needs to be null. Otherwise the divider color could still be set if
        // needsDarkButtons is true.
        when(mColorProvider.getNavigationBarColor()).thenReturn(null);

        CustomTabNavigationBarController.update(
                mWindow, mCustomTabIntentDataProvider, mContext, /* isEdgeToEdge= */ false);
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
        CustomTabNavigationBarController.update(
                mWindow, mCustomTabIntentDataProvider, mContext, /* isEdgeToEdge= */ false);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.O) // SDK 26 is used to trigger supportDarkButtons=true.
    public void setsCorrectBarColorWhenDarkButtonsSupported() {
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(Color.RED);
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.GREEN);

        // The case when needsDarkButtons=true
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.WHITE);
        CustomTabNavigationBarController.update(
                mWindow, mCustomTabIntentDataProvider, mContext, /* isEdgeToEdge= */ false);
        verify(mWindow).setNavigationBarColor(Color.WHITE);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P) // Android P+ (>=28) needed for setting divider color.
    public void setsCorrectDividerColor() {
        // The case when divider color is set explicitly.
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(Color.RED);
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.BLACK);

        CustomTabNavigationBarController.update(
                mWindow, mCustomTabIntentDataProvider, mContext, /* isEdgeToEdge= */ false);
        verify(mWindow).setNavigationBarDividerColor(Color.RED);

        // The case when divider color is set implicitly due to needsDarkButtons=true.
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(null);
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.WHITE);
        CustomTabNavigationBarController.update(
                mWindow, mCustomTabIntentDataProvider, mContext, /* isEdgeToEdge= */ false);
        verify(mWindow)
                .setNavigationBarDividerColor(
                        mContext.getColor(org.chromium.chrome.R.color.black_alpha_12));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P) // Android P+ (>=28) needed for setting divider color.
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR)
    public void setsCorrectDividerColorWhenGoogleBottomBarEnabled() {
        when(mConnection.shouldEnableGoogleBottomBarForIntent(mCustomTabIntentDataProvider))
                .thenReturn(true);
        CustomTabNavigationBarController.update(
                mWindow, mCustomTabIntentDataProvider, mContext, /* isEdgeToEdge= */ false);
        verify(mWindow)
                .setNavigationBarColor(
                        mContext.getColor(R.color.google_bottom_bar_background_color));
        verify(mWindow)
                .setNavigationBarDividerColor(
                        mContext.getColor(R.color.google_bottom_bar_background_color));
    }

    @Test
    public void setTransparentColorForEdgeToEdge() {
        CustomTabNavigationBarController.update(
                mWindow, mCustomTabIntentDataProvider, mContext, /* isEdgeToEdge= */ true);
        verify(mWindow).setNavigationBarColor(Color.TRANSPARENT);
    }
}
