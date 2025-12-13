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
import android.view.Window;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;

/** Tests for {@link CustomTabNavigationBarController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class CustomTabNavigationBarControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ColorProvider mColorProvider;
    @Mock private CustomTabIntentDataProvider mCustomTabIntentDataProvider;
    @Mock private CustomTabsConnection mConnection;
    @Mock private EdgeToEdgeSystemBarColorHelper mSystemBarColorHelper;
    private Window mWindow;
    private Context mContext;

    @Before
    public void setUp() {
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
                mWindow,
                mCustomTabIntentDataProvider,
                mContext,
                /* isEdgeToEdge= */ false,
                mSystemBarColorHelper);

        verify(mWindow, never()).setNavigationBarColor(Mockito.anyInt());
        verify(mSystemBarColorHelper, never()).setNavigationBarColor(Mockito.anyInt());
    }

    @Test
    public void doesNotSetDividerColorWhenNull() {
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(null);
        // Bar color needs to be null. Otherwise the divider color could still be set if
        // needsDarkButtons is true.
        when(mColorProvider.getNavigationBarColor()).thenReturn(null);

        CustomTabNavigationBarController.update(
                mWindow,
                mCustomTabIntentDataProvider,
                mContext,
                /* isEdgeToEdge= */ false,
                mSystemBarColorHelper);
        verify(mWindow, never()).setNavigationBarDividerColor(Mockito.anyInt());
        verify(mSystemBarColorHelper, never()).setNavigationBarDividerColor(Mockito.anyInt());
    }

    @Test
    public void setsCorrectBarColorWhenDarkButtonsSupported() {
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(Color.RED);
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.GREEN);

        // The case when needsDarkButtons=true
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.WHITE);
        CustomTabNavigationBarController.update(
                mWindow,
                mCustomTabIntentDataProvider,
                mContext,
                /* isEdgeToEdge= */ false,
                mSystemBarColorHelper);
        verify(mSystemBarColorHelper).setNavigationBarColor(Color.WHITE);
    }

    @Test
    public void setsCorrectDividerColor() {
        // The case when divider color is set explicitly.
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(Color.RED);
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.BLACK);

        CustomTabNavigationBarController.update(
                mWindow,
                mCustomTabIntentDataProvider,
                mContext,
                /* isEdgeToEdge= */ false,
                mSystemBarColorHelper);
        verify(mSystemBarColorHelper).setNavigationBarDividerColor(Color.RED);

        // The case when divider color is set implicitly due to needsDarkButtons=true.
        when(mColorProvider.getNavigationBarDividerColor()).thenReturn(null);
        when(mColorProvider.getNavigationBarColor()).thenReturn(Color.WHITE);
        CustomTabNavigationBarController.update(
                mWindow,
                mCustomTabIntentDataProvider,
                mContext,
                /* isEdgeToEdge= */ false,
                mSystemBarColorHelper);
        verify(mSystemBarColorHelper)
                .setNavigationBarDividerColor(mContext.getColor(R.color.black_alpha_12));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR)
    public void setsCorrectDividerColorWhenGoogleBottomBarEnabled() {
        when(mConnection.shouldEnableGoogleBottomBarForIntent(mCustomTabIntentDataProvider))
                .thenReturn(true);
        CustomTabNavigationBarController.update(
                mWindow,
                mCustomTabIntentDataProvider,
                mContext,
                /* isEdgeToEdge= */ false,
                mSystemBarColorHelper);
        verify(mSystemBarColorHelper)
                .setNavigationBarColor(
                        mContext.getColor(R.color.google_bottom_bar_background_color));
        verify(mSystemBarColorHelper)
                .setNavigationBarDividerColor(
                        mContext.getColor(R.color.google_bottom_bar_background_color));
    }

    @Test
    public void setTransparentColorForEdgeToEdge() {
        CustomTabNavigationBarController.update(
                mWindow,
                mCustomTabIntentDataProvider,
                mContext,
                /* isEdgeToEdge= */ true,
                mSystemBarColorHelper);
        verify(mSystemBarColorHelper).setNavigationBarColor(Color.TRANSPARENT);
    }
}
