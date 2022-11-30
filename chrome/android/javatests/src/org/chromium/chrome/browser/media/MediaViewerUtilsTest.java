// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.TestContentProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.util.ColorUtils;

/**
 * Integration test suite for the MediaViewerUtils.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MediaViewerUtilsTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Test
    @MediumTest
    public void testCustomTabActivityInLightMode() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntentWithTheme(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, /* inNightMode= */ false));

        Uri uri = Uri.parse(TestContentProvider.createContentUrl("google.png"));
        Intent intent = MediaViewerUtils.getMediaViewerIntent(uri, uri, "image/png",
                false /*allowExternalAppHandlers */, mCustomTabActivityTestRule.getActivity());
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Assert.assertFalse(ColorUtils.inNightMode(mCustomTabActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    public void testCustomTabActivityInDarkMode() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntentWithTheme(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, /* inNightMode= */ true));

        Uri uri = Uri.parse(TestContentProvider.createContentUrl("google.png"));
        Intent intent = MediaViewerUtils.getMediaViewerIntent(uri, uri, "image/png",
                false /*allowExternalAppHandlers */, mCustomTabActivityTestRule.getActivity());
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Assert.assertTrue(ColorUtils.inNightMode(mCustomTabActivityTestRule.getActivity()));
    }
}