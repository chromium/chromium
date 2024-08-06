// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils.createMinimalCustomTabIntent;
import static org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils.createMinimalIncognitoCustomTabIntent;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;

import org.hamcrest.Matchers;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * This class provides helper methods for launching any Urls in CCT or Tabs. This also provides
 * parameters for tests. Parameters include pair of activity types.
 */
public class IncognitoDataTestUtils {
    public enum ActivityType {
        INCOGNITO_TAB(true, false),
        INCOGNITO_CCT(true, true),
        REGULAR_TAB(false, false),
        REGULAR_CCT(false, true);

        public final boolean incognito;
        public final boolean cct;

        ActivityType(boolean incognito, boolean cct) {
            this.incognito = incognito;
            this.cct = cct;
        }

        public Tab launchUrl(
                ChromeTabbedActivityTestRule chromeTabbedActivityRule,
                CustomTabActivityTestRule customTabActivityTestRule,
                String url) {
            if (cct) {
                return launchUrlInCCT(customTabActivityTestRule, url, incognito);
            } else {
                return launchUrlInTab(chromeTabbedActivityRule, url, incognito);
            }
        }
    }

    /**
     * A class providing test parameters encapsulating different Activity type pairs spliced on
     * regular and Incognito mode. This is used for tests which check leakages to/from Incognito.
     */
    public static class TestParams {
        private static List<ParameterSet> getParameters(
                boolean firstIncognito, boolean secondIncognito) {
            List<ParameterSet> tests = new ArrayList<>();

            for (ActivityType activity1 : ActivityType.values()) {
                for (ActivityType activity2 : ActivityType.values()) {
                    // We remove the test with two incognito tabs because they are known to share
                    // state via same session.
                    if ((activity1.incognito && !activity1.cct)
                            && (activity2.incognito && !activity2.cct)) {
                        continue;
                    }

                    if (activity1.incognito == firstIncognito
                            && activity2.incognito == secondIncognito) {
                        tests.add(
                                new ParameterSet()
                                        .value(activity1.toString(), activity2.toString())
                                        .name(activity1.toString() + "_" + activity2.toString()));
                    }
                }
            }

            return tests;
        }

        /**
         * A class providing test parameters encapsulating different Activity type pairs where the
         * Activity from which we check the leak from is Regular mode, and the leak to is Incognito
         * mode.
         */
        public static class RegularToIncognito implements ParameterProvider {
            @Override
            public List<ParameterSet> getParameters() {
                return TestParams.getParameters(false, true);
            }
        }

        /**
         * A class providing test parameters encapsulating different Activity type pairs where the
         * Activity from which we check the leak from is Incognito mode, and the leak to is Regular
         * mode.
         */
        public static class IncognitoToRegular implements ParameterProvider {
            @Override
            public List<ParameterSet> getParameters() {
                return TestParams.getParameters(true, false);
            }
        }

        /**
         * A class providing test parameters encapsulating different Activity type pairs where the
         * Activity from which we check the leak from is Incognito mode, and the leak to is also
         * Incognito mode.
         */
        public static class IncognitoToIncognito implements ParameterProvider {
            @Override
            public List<ParameterSet> getParameters() {
                return TestParams.getParameters(true, true);
            }
        }

        /**
         * A class providing test parameters encapsulating different Activity type pairs where the
         * Activity from which we check the leak from is Regular mode, and the leak to is also
         * Regular mode.
         */
        public static class RegularToRegular implements ParameterProvider {
            @Override
            public List<ParameterSet> getParameters() {
                return TestParams.getParameters(false, false);
            }
        }

        /**
         * A general class providing test parameters encapsulating different Activity type pairs
         * spliced on Regular and Incognito mod between whom we want to test leakage.
         */
        public static class AllTypesToAllTypes implements ParameterProvider {
            @Override
            public List<ParameterSet> getParameters() {
                List<ParameterSet> result = new ArrayList<>();
                result.addAll(new TestParams.RegularToIncognito().getParameters());
                result.addAll(new TestParams.IncognitoToRegular().getParameters());
                result.addAll(new TestParams.IncognitoToIncognito().getParameters());
                result.addAll(new TestParams.RegularToRegular().getParameters());
                return result;
            }
        }
    }

    private static boolean isChromeTabbedActivityRunningOnTop() {
        Activity topActivity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (topActivity == null) return false;
        return (topActivity instanceof ChromeTabbedActivity);
    }

    private static Tab launchUrlInTab(
            ChromeTabbedActivityTestRule testRule, String url, boolean incognito) {
        // This helps to bring back the "existing" chrome tabbed activity to foreground
        // in case the custom tab activity was launched before.
        if (!isChromeTabbedActivityRunningOnTop()) {
            testRule.startMainActivityOnBlankPage();
        }

        Tab tab = testRule.loadUrlInNewTab(url, incognito);

        // Giving time to the WebContents to be ready.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(tab.getWebContents(), Matchers.notNullValue()));

        assertEquals(incognito, tab.getWebContents().isIncognito());
        return tab;
    }

    private static Tab launchUrlInCCT(
            CustomTabActivityTestRule testRule, String url, boolean incognito) {
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                incognito
                        ? createMinimalIncognitoCustomTabIntent(context, url)
                        : createMinimalCustomTabIntent(context, url);

        testRule.startCustomTabActivityWithIntent(intent);
        Tab tab = testRule.getActivity().getActivityTab();

        // Giving time to the WebContents to be ready.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(tab.getWebContents(), Matchers.notNullValue()));

        assertEquals(incognito, tab.getWebContents().isIncognito());
        return tab;
    }

    public static void closeTabs(ChromeActivityTestRule testRule) {
        ChromeActivity activity = testRule.getActivity();
        if (activity == null) return;
        activity.getTabModelSelector()
                .getModel(false)
                .closeTabs(TabClosureParams.closeAllTabs().build());
        activity.getTabModelSelector()
                .getModel(true)
                .closeTabs(TabClosureParams.closeAllTabs().build());
    }

    // Warming up CCT so that the native is initialized before we access feature flags.
    public static void fireAndWaitForCctWarmup() throws TimeoutException {
        CallbackHelper startUpCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowserStartupController.getInstance()
                            .addStartupCompletedObserver(
                                    new BrowserStartupController.StartupCallback() {
                                        @Override
                                        public void onSuccess() {
                                            startUpCallback.notifyCalled();
                                        }

                                        @Override
                                        public void onFailure() {
                                            // Need a successful startup for test.
                                            assert false;
                                        }
                                    });
                });

        CustomTabsConnection.getInstance().warmup(0);
        startUpCallback.waitForCallback(0);
    }
}
