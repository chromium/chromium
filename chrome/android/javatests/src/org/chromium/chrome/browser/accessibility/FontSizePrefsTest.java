// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs;

/**
 * Tests for {@link FontSizePrefs}.
 *
 * <p>TODO(crbug.com/40214849): This tests the class in //components/browser_ui, but we don't have a
 * good way of testing with native code there.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class FontSizePrefsTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private FontSizePrefs mFontSizePrefs;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mFontSizePrefs = getFontSizePrefs(context);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testForceEnableZoom() {
        Assert.assertFalse(getForceEnableZoom());
        setForceEnableZoom(true);
        Assert.assertTrue(getForceEnableZoom());
    }

    private FontSizePrefs getFontSizePrefs(final Context context) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> FontSizePrefs.getInstance(ProfileManager.getLastUsedRegularProfile()));
    }

    private void setForceEnableZoom(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(() -> mFontSizePrefs.setForceEnableZoom(enabled));
    }

    private boolean getForceEnableZoom() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mFontSizePrefs.getForceEnableZoom());
    }
}
