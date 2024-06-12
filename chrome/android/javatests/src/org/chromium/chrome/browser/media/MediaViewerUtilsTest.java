// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Intent;
import android.net.Uri;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.TestContentProvider;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.util.ColorUtils;

/** Integration test suite for the MediaViewerUtils. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MediaViewerUtilsTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Test
    @MediumTest
    public void testCustomTabActivityInLightMode() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntentWithTheme(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                        /* inNightMode= */ false));

        Uri uri = Uri.parse(TestContentProvider.createContentUrl("google.png"));
        Intent intent =
                MediaViewerUtils.getMediaViewerIntent(
                        uri,
                        uri,
                        "image/png",
                        /* allowExternalAppHandlers= */ false,
                        /* allowShareAction= */ true,
                        mCustomTabActivityTestRule.getActivity());
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Assert.assertFalse(ColorUtils.inNightMode(mCustomTabActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    public void testCustomTabActivityInDarkMode() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntentWithTheme(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                        /* inNightMode= */ true));

        Uri uri = Uri.parse(TestContentProvider.createContentUrl("google.png"));
        Intent intent =
                MediaViewerUtils.getMediaViewerIntent(
                        uri,
                        uri,
                        "image/png",
                        /* allowExternalAppHandlers= */ false,
                        /* allowShareAction= */ true,
                        mCustomTabActivityTestRule.getActivity());
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Assert.assertTrue(ColorUtils.inNightMode(mCustomTabActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    public void testViewMediaWithoutShareAction() {
        Uri uri = Uri.parse(TestContentProvider.createContentUrl("google.png"));
        Intent intent =
                MediaViewerUtils.getMediaViewerIntent(
                        uri,
                        uri,
                        "image/png",
                        /* allowExternalAppHandlers= */ false,
                        /* allowShareAction= */ false,
                        InstrumentationRegistry.getInstrumentation().getContext());
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        mCustomTabActivityTestRule.waitForActivityCompletelyLoaded();

        ViewGroup customActionButtons =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.action_buttons);
        Assert.assertEquals(
                "allowShareAction = false will lead to no custom action being added.",
                0,
                customActionButtons.getChildCount());
        Assert.assertNull(
                "allowExternalAppHandlers = false will lead to 0 menu items in CCT. "
                        + "Menu button should be hidden.",
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.menu_button_wrapper));
    }
}
