// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTypeTestUtils;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeTabUtils;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link CustomTabActivityTabController}.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabActivityTabControllerTest {
    // Do not test TWAs because {@link CustomTabActivityTypeTestUtils#launchActivity} warms up the
    // {@link CustomTabsConnection} which bypasses {@link StartupTabPreloader}.
    @ClassParameter
    public static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(ActivityType.WEBAPP).name("Webapp"),
                    new ParameterSet().value(ActivityType.CUSTOM_TAB).name("CustomTab"));

    private @ActivityType int mActivityType;

    @Rule
    public final ChromeActivityTestRule<? extends BaseCustomTabActivity> mActivityTestRule;

    public CustomTabActivityTabControllerTest(@ActivityType int activityType) {
        mActivityType = activityType;
        mActivityTestRule = CustomTabActivityTypeTestUtils.createActivityTestRule(activityType);
    }

    /**
     * Test that the {@link StartupTabPreloader} tab is used by default.
     */
    @Test
    @MediumTest
    @Feature({"CustomTabs"})
    public void testUseStartupPreloaderTab() throws TimeoutException {
        CustomTabActivityTypeTestUtils.launchActivity(
                mActivityType, mActivityTestRule, "about:blank");
        CustomTabActivityTabProvider tabProvider = getActivityTabProvider();
        assertEquals(TabCreationMode.FROM_STARTUP_TAB_PRELOADER,
                tabProvider.getInitialTabCreationMode());
    }

    /**
     * Test that the {@link StartupTabPreloader} tab is not used if the preloaded URL is different
     * than
     * {@link IntentDataProvider#getUrlToLoad()}.
     */
    @Test
    @MediumTest
    public void testDontUseStartupPreloaderMediaViewerUrl() throws TimeoutException {
        if (mActivityType != ActivityType.CUSTOM_TAB) return;

        String mediaViewerUrl = UrlUtils.getTestFileUrl("google.png");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), "about:blank");
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.MEDIA_VIEWER);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_MEDIA_VIEWER_URL, mediaViewerUrl);
        IntentHandler.addTrustedIntentExtras(intent);
        ((CustomTabActivityTestRule) mActivityTestRule).startCustomTabActivityWithIntent(intent);

        CustomTabActivityTabProvider tabProvider = getActivityTabProvider();
        assertEquals(
                mediaViewerUrl, ChromeTabUtils.getUrlOnUiThread(tabProvider.getTab()).getSpec());
        assertNotEquals(TabCreationMode.FROM_STARTUP_TAB_PRELOADER,
                tabProvider.getInitialTabCreationMode());
    }

    private CustomTabActivityTabProvider getActivityTabProvider() {
        return mActivityTestRule.getActivity().getComponent().resolveTabProvider();
    }
}
