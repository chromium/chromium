// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.system.StatusBarColorController.DEFAULT_STATUS_BAR_COLOR;
import static org.chromium.chrome.browser.ui.system.StatusBarColorController.UNDEFINED_STATUS_BAR_COLOR;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;

/** Tests for {@link CustomTabStatusBarColorProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class CustomTabStatusBarColorProviderTest {
    private static final int USER_PROVIDED_COLOR = 0x99aabbcc;

    @Mock public CustomTabIntentDataProvider mCustomTabIntentDataProvider;
    @Mock public StatusBarColorController mStatusBarColorController;
    @Mock public Tab mTab;
    private CustomTabStatusBarColorProvider mStatusBarColorProvider;
    @Mock private ColorProvider mColorProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mStatusBarColorProvider =
                Mockito.spy(
                        new CustomTabStatusBarColorProvider(
                                mCustomTabIntentDataProvider, mStatusBarColorController));

        when(mCustomTabIntentDataProvider.getColorProvider()).thenReturn(mColorProvider);

        when(mColorProvider.getToolbarColor()).thenReturn(USER_PROVIDED_COLOR);
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(true);
    }

    @Test
    public void undefinedWhenOpenedByChromeNoCustom() {
        when(mCustomTabIntentDataProvider.isOpenedByChrome()).thenReturn(true);
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(false);
        Assert.assertEquals(UNDEFINED_STATUS_BAR_COLOR, getStatusBarColor(mTab));
    }

    @Test
    public void openedByChromeWithCustom() {
        when(mCustomTabIntentDataProvider.isOpenedByChrome()).thenReturn(true);
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(true);
        Assert.assertEquals(USER_PROVIDED_COLOR, getStatusBarColor(mTab));
    }

    @Test
    public void useTabThemeColor_enable() {
        mStatusBarColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(UNDEFINED_STATUS_BAR_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController).updateStatusBarColor();
    }

    @Test
    public void useTabThemeColor_enable_nullTab() {
        mStatusBarColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(USER_PROVIDED_COLOR, getStatusBarColor(null));

        when(mColorProvider.hasCustomToolbarColor()).thenReturn(false);
        Assert.assertEquals(DEFAULT_STATUS_BAR_COLOR, getStatusBarColor(null));
    }

    @Test
    public void useTabThemeColor_disable() {
        mStatusBarColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(UNDEFINED_STATUS_BAR_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController).updateStatusBarColor();

        mStatusBarColorProvider.setUseTabThemeColor(false);
        Assert.assertEquals(USER_PROVIDED_COLOR, getStatusBarColor(mTab));
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

        Assert.assertEquals(UNDEFINED_STATUS_BAR_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController).updateStatusBarColor();
    }

    private int getStatusBarColor(Tab tab) {
        return mStatusBarColorProvider.getBaseStatusBarColor(tab);
    }
}
