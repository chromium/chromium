// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link IncognitoTabLauncher}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.ALLOW_NEW_INCOGNITO_TAB_INTENTS})
public class IncognitoTabLauncherTest {
    @Test
    @Feature("Incognito")
    @SmallTest
    public void testEnableComponent() {
        Context context = InstrumentationRegistry.getTargetContext();
        IncognitoTabLauncher.setComponentEnabled(true);
        Assert.assertNotNull(
                context.getPackageManager().resolveActivity(createLaunchIntent(context), 0));
    }

    @Test
    @Feature("Incognito")
    @SmallTest
    public void testDisableComponent() {
        Context context = InstrumentationRegistry.getTargetContext();
        IncognitoTabLauncher.setComponentEnabled(false);
        Assert.assertNull(
                context.getPackageManager().resolveActivity(createLaunchIntent(context), 0));
    }

    @Test
    @Feature("Incognito")
    @MediumTest
    public void testLaunchIncognitoNewTab() {
        ChromeTabbedActivity activity = launchIncognitoTab();

        Assert.assertTrue(activity.getTabModelSelector().isIncognitoSelected());
        Assert.assertTrue(IncognitoTabLauncher.didCreateIntent(activity.getIntent()));
    }

    @Test
    @Feature("Incognito")
    @MediumTest
    public void testLaunchIncognitoNewTab_omniboxFocused() {
        ChromeTabbedActivity activity = launchIncognitoTab();
        CriteriaHelper.pollUiThread(() -> activity.getToolbarManager().isUrlBarFocused());
    }

    private ChromeTabbedActivity launchIncognitoTab() {
        Context context = InstrumentationRegistry.getTargetContext();
        IncognitoTabLauncher.setComponentEnabled(true);
        Intent intent = createLaunchIntent(context);

        // We need FLAG_ACTIVITY_NEW_TASK because we're calling from the application context (not an
        // Activity context). This is fine though because ChromeActivityTestRule.waitFor uses
        // ApplicationStatus internally, which ignores Tasks and tracks all Chrome Activities.
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        TestThreadUtils.runOnUiThreadBlocking(() -> context.startActivity(intent));

        return ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class);
    }

    private Intent createLaunchIntent(Context context) {
        Intent intent = new Intent(IncognitoTabLauncher.ACTION_LAUNCH_NEW_INCOGNITO_TAB);
        // Restrict ourselves to Chrome's package, on the off chance the testing device has another
        // application that answers to the ACTION_LAUNCH_NEW_INCOGNITO_TAB action.
        intent.setPackage(context.getPackageName());

        return intent;
    }
}