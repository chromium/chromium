// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.system.StatusBarColorController.UNDEFINED_STATUS_BAR_COLOR;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
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
    @Mock
    public ActivityTabProvider mActivityTabProvider;
    @Mock public StatusBarColorController mStatusBarColorController;
    @Mock public Tab mTab;
    private CustomTabStatusBarColorProvider mColorProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mColorProvider = new CustomTabStatusBarColorProvider(
                mCustomTabIntentDataProvider, mActivityTabProvider, mStatusBarColorController);

        when(mCustomTabIntentDataProvider.getToolbarColor()).thenReturn(USER_PROVIDED_COLOR);

        when(mActivityTabProvider.get()).thenReturn(mTab);
    }

    @Test
    public void fallsBackWhenOpenedByChrome() {
        when(mCustomTabIntentDataProvider.isOpenedByChrome()).thenReturn(true);

        Assert.assertEquals(FALLBACK_COLOR, getStatusBarColor());

        Assert.assertTrue(mColorProvider.isStatusBarDefaultThemeColor(true));
        Assert.assertFalse(mColorProvider.isStatusBarDefaultThemeColor(false));
    }

    @Test
    public void userProvidedColor() {
        Assert.assertEquals(USER_PROVIDED_COLOR, getStatusBarColor());
        Assert.assertFalse(mColorProvider.isStatusBarDefaultThemeColor(true));
    }

    @Test
    public void useTabThemeColor_enable() {
        mColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(UNDEFINED_STATUS_BAR_COLOR, getStatusBarColor());
        verify(mStatusBarColorController).updateStatusBarColor(any(Tab.class));
    }

    @Test
    public void useTabThemeColor_disable() {
        mColorProvider.setUseTabThemeColor(true);
        Assert.assertEquals(UNDEFINED_STATUS_BAR_COLOR, getStatusBarColor());
        verify(mStatusBarColorController).updateStatusBarColor(any(Tab.class));

        mColorProvider.setUseTabThemeColor(false);
        Assert.assertEquals(USER_PROVIDED_COLOR, getStatusBarColor());
        verify(mStatusBarColorController, times(2)).updateStatusBarColor(any(Tab.class));
    }

    @Test
    public void useTabThemeColor_idempotent() {
        mColorProvider.setUseTabThemeColor(true);
        mColorProvider.setUseTabThemeColor(true);

        Assert.assertEquals(UNDEFINED_STATUS_BAR_COLOR, getStatusBarColor());
        verify(mStatusBarColorController).updateStatusBarColor(any(Tab.class));
    }

    private int getStatusBarColor() {
        return mColorProvider.getBaseStatusBarColor(FALLBACK_COLOR);
    }
}
