// Copyright 2019 The Chromium Authors. All rights reserved.
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;

/**
 * Tests for {@link CustomTabStatusBarColorProvider}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomTabStatusBarColorProviderTest {
    private static final int DEFAULT_COLOR = 0x11223344;
    private static final int FALLBACK_COLOR = 0x55667788;
    private static final int USER_PROVIDED_COLOR = 0x99aabbcc;

    @Mock public CustomTabIntentDataProvider mCustomTabIntentDataProvider;
    @Mock public StatusBarColorController mStatusBarColorController;
    @Mock
    public TabImpl mTab;
    private CustomTabStatusBarColorProvider mColorProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mColorProvider = Mockito.spy(new CustomTabStatusBarColorProvider(
                mCustomTabIntentDataProvider, mStatusBarColorController));

        when(mCustomTabIntentDataProvider.getToolbarColor()).thenReturn(USER_PROVIDED_COLOR);
        when(mCustomTabIntentDataProvider.hasCustomToolbarColor()).thenReturn(true);
    }

    @Test
    public void fallsBackWhenOpenedByChrome() {
        when(mCustomTabIntentDataProvider.isOpenedByChrome()).thenReturn(true);

        Assert.assertEquals(FALLBACK_COLOR, getStatusBarColor(mTab));
    }

    @Test
    public void useTabThemeColor_enable() {
        mColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(UNDEFINED_STATUS_BAR_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController).updateStatusBarColor();
    }

    @Test
    public void useTabThemeColor_enable_nullTab() {
        mColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(USER_PROVIDED_COLOR, getStatusBarColor(null));

        when(mCustomTabIntentDataProvider.hasCustomToolbarColor()).thenReturn(false);
        Assert.assertEquals(DEFAULT_STATUS_BAR_COLOR, getStatusBarColor(null));
    }

    @Test
    public void useTabThemeColor_disable() {
        mColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(UNDEFINED_STATUS_BAR_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController).updateStatusBarColor();

        mColorProvider.setUseTabThemeColor(false);
        Assert.assertEquals(USER_PROVIDED_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController, times(2)).updateStatusBarColor();
    }

    @Test
    public void useTabThemeColor_disable_noCustomColor() {
        when(mCustomTabIntentDataProvider.hasCustomToolbarColor()).thenReturn(false);
        mColorProvider.setUseTabThemeColor(false);
        Assert.assertEquals(DEFAULT_STATUS_BAR_COLOR, getStatusBarColor(mTab));
    }

    @Test
    public void useTabThemeColor_idempotent() {
        mColorProvider.setUseTabThemeColor(true);
        mColorProvider.setUseTabThemeColor(true);

        Assert.assertEquals(UNDEFINED_STATUS_BAR_COLOR, getStatusBarColor(mTab));
        verify(mStatusBarColorController).updateStatusBarColor();
    }

    private int getStatusBarColor(Tab tab) {
        return mColorProvider.getBaseStatusBarColor(tab, FALLBACK_COLOR);
    }
}
