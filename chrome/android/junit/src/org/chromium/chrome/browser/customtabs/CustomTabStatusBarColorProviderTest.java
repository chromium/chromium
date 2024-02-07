// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.system.StatusBarColorController.DEFAULT_STATUS_BAR_COLOR;

import android.graphics.Color;

import androidx.annotation.ColorInt;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;

/** Tests for {@link CustomTabStatusBarColorProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabStatusBarColorProviderTest {
    private static final @ColorInt int TOOLBAR_COLOR = Color.YELLOW;
    private static final @ColorInt int TAB_THEME_COLOR = Color.MAGENTA;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private CustomTabIntentDataProvider mCustomTabIntentDataProvider;
    @Mock private StatusBarColorController mStatusBarColorController;
    @Mock private Tab mTab;
    @Mock private ColorProvider mColorProvider;
    @Mock private TopUiThemeColorProvider mThemeColorProvider;
    private CustomTabStatusBarColorProvider mStatusBarColorProvider;

    @Before
    public void setUp() {
        mStatusBarColorProvider =
                Mockito.spy(
                        new CustomTabStatusBarColorProvider(
                                mCustomTabIntentDataProvider,
                                mThemeColorProvider,
                                mStatusBarColorController));

        doReturn(TAB_THEME_COLOR)
                .when(mThemeColorProvider)
                .getThemeColorOrFallback(any(), anyInt());

        when(mCustomTabIntentDataProvider.getColorProvider()).thenReturn(mColorProvider);

        when(mColorProvider.getToolbarColor()).thenReturn(TOOLBAR_COLOR);
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(true);
    }

    @Test
    public void undefinedWhenOpenedByChromeNoCustom() {
        when(mCustomTabIntentDataProvider.isOpenedByChrome()).thenReturn(true);
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(false);
        Assert.assertEquals(TAB_THEME_COLOR, getStatusBarColor(mTab));
    }

    @Test
    public void openedByChromeWithCustom() {
        when(mCustomTabIntentDataProvider.isOpenedByChrome()).thenReturn(true);
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(true);
        Assert.assertEquals(TOOLBAR_COLOR, getStatusBarColor(mTab));
    }

    @Test
    public void useTabThemeColor_enable() {
        mStatusBarColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(TAB_THEME_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController).updateStatusBarColor();
    }

    @Test
    public void useTabThemeColor_enable_nullTab() {
        mStatusBarColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(TOOLBAR_COLOR, getStatusBarColor(null));

        when(mColorProvider.hasCustomToolbarColor()).thenReturn(false);
        Assert.assertEquals(DEFAULT_STATUS_BAR_COLOR, getStatusBarColor(null));
    }

    @Test
    public void useTabThemeColor_disable() {
        mStatusBarColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(TAB_THEME_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController).updateStatusBarColor();

        mStatusBarColorProvider.setUseTabThemeColor(false);
        Assert.assertEquals(TOOLBAR_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController, times(2)).updateStatusBarColor();
    }

    @Test
    public void useTabThemeColor_disable_noCustomColor() {
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(false);
        mStatusBarColorProvider.setUseTabThemeColor(false);
        Assert.assertEquals(DEFAULT_STATUS_BAR_COLOR, getStatusBarColor(mTab));
    }

    @Test
    public void useTabThemeColor_idempotent() {
        mStatusBarColorProvider.setUseTabThemeColor(true);
        mStatusBarColorProvider.setUseTabThemeColor(true);

        Assert.assertEquals(TAB_THEME_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController).updateStatusBarColor();
    }

    private int getStatusBarColor(Tab tab) {
        return mStatusBarColorProvider.getBaseStatusBarColor(tab);
    }
}
