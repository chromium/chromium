// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;
import static org.junit.Assert.assertThat;

import android.annotation.TargetApi;
import android.content.Intent;
import android.os.Build;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;

/**
 * Tests the availability of core direct actions in different activities.
 *
 * <p>This tests both {@link DirectActionInitializer} and its integration with {@link
 * ChromeActivity} and its different subclasses.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_DIRECT_ACTIONS)
@MinAndroidSdkLevel(Build.VERSION_CODES.N)
@TargetApi(24) // For java.util.function.Consumer.
public class DirectActionAvailabilityCustomTabTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public DirectActionTestRule mDirectActionRule = new DirectActionTestRule();

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testCoreDirectActionInCustomTabActivity() throws Exception {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), "about:blank");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertThat(
                DirectActionTestUtils.setupActivityAndGetDirectAction(mCustomTabActivityTestRule),
                Matchers.containsInAnyOrder("go_back", "reload", "go_forward", "bookmark_this_page",
                        "preferences", "find_in_page"));
    }
}
