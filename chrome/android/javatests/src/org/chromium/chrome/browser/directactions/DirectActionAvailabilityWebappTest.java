// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;
import static org.junit.Assert.assertThat;

import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.autofill_assistant.AssistantFeatures;

/**
 * Tests the availability of core direct actions in different activities.
 *
 * <p>This tests both {@link DirectActionInitializer} and its integration with {@link
 * ChromeActivity} and its different subclasses.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_DIRECT_ACTIONS_NAME)
@MinAndroidSdkLevel(Build.VERSION_CODES.N)
@RequiresApi(24) // For java.util.function.Consumer.
public class DirectActionAvailabilityWebappTest {
    @Rule
    public WebappActivityTestRule mWebAppActivityTestRule = new WebappActivityTestRule();

    @Rule
    public DirectActionTestRule mDirectActionRule = new DirectActionTestRule();

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testCoreDirectActionInWebappActivity() throws Exception {
        mWebAppActivityTestRule.startWebappActivity();

        assertThat(DirectActionTestUtils.setupActivityAndGetDirectAction(mWebAppActivityTestRule),
                Matchers.containsInAnyOrder("go_back", "reload", "go_forward", "find_in_page"));
    }
}
