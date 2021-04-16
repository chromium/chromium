// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.TestContentProvider;
import org.chromium.chrome.test.util.browser.Features;

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

    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({ChromeSwitches.FORCE_ENABLE_NIGHT_MODE})
    public void testCustomTabActivityInDarkMode() throws Exception {
        Uri uri = Uri.parse(TestContentProvider.createContentUrl("google.png"));
        Intent intent = MediaViewerUtils.getMediaViewerIntent(
                uri, uri, "image/png", false /*allowExternalAppHandlers */);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        CustomTabActivity cta = mCustomTabActivityTestRule.getActivity();
        Assert.assertNotNull(cta.getNightModeStateProviderForTesting());
        Assert.assertTrue(cta.getNightModeStateProviderForTesting().isInNightMode());
    }
}
