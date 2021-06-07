// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.R;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link OmniboxResourceProvider}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxResourceProviderTest {
    @Test
    public void getThemeFromDarkColorsAndIncognito() {
        @OmniboxTheme
        int actual = OmniboxResourceProvider.getThemeFromDarkColorsAndIncognito(
                /*useDarkColors=*/true, /*isIncognito=*/false);
        Assert.assertEquals(OmniboxTheme.LIGHT_THEME, actual);

        actual = OmniboxResourceProvider.getThemeFromDarkColorsAndIncognito(
                /*useDarkColors=*/false, /*isIncognito=*/false);
        Assert.assertEquals(OmniboxTheme.DARK_THEME, actual);

        actual = OmniboxResourceProvider.getThemeFromDarkColorsAndIncognito(
                /*useDarkColors=*/false, /*isIncognito=*/true);
        Assert.assertEquals(OmniboxTheme.INCOGNITO, actual);

        actual = OmniboxResourceProvider.getThemeFromDarkColorsAndIncognito(
                /*useDarkColors=*/true, /*isIncognito=*/true);
        Assert.assertEquals(OmniboxTheme.INCOGNITO, actual);
    }

    @Test
    public void isDarkMode() {
        Assert.assertTrue(OmniboxResourceProvider.isDarkMode(OmniboxTheme.DARK_THEME));
        Assert.assertTrue(OmniboxResourceProvider.isDarkMode(OmniboxTheme.INCOGNITO));
        Assert.assertFalse(OmniboxResourceProvider.isDarkMode(OmniboxTheme.LIGHT_THEME));
    }

    @Test
    public void resolveAttributeToDrawable() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        // First set the app theme, then apply the feed theme overlay.
        activity.setTheme(R.style.Theme_BrowserUI);
        activity.setTheme(R.style.ThemeOverlay_Feed_Light);

        Drawable drawableLight = OmniboxResourceProvider.resolveAttributeToDrawable(
                activity, OmniboxTheme.LIGHT_THEME, R.attr.selectableItemBackground);
        Assert.assertNotNull(drawableLight);

        activity.setTheme(R.style.ThemeOverlay_Feed_Dark);
        Drawable drawableDark = OmniboxResourceProvider.resolveAttributeToDrawable(
                activity, OmniboxTheme.DARK_THEME, R.attr.selectableItemBackground);
        Assert.assertNotNull(drawableDark);
    }
}
