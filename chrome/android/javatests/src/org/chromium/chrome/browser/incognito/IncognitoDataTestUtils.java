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

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TrafficControl;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Supplier;

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
                FreshCtaTransitTestRule chromeTabbedActivityRule,
                CustomTabActivityTestRule customTabActivityTestRule,
                String url) {
            if (cct) {
                return launchUrlInCct(customTabActivityTestRule, url, incognito);
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

    private static void moveTaskToFrontIfPausedOrStopped(
            Activity activity, @ActivityState int state) {
        if (state == ActivityState.PAUSED || state == ActivityState.STOPPED) {
            ApiCompatibilityUtils.moveTaskToFront(activity, activity.getTaskId(), 0);
        }
    }

    private static Tab runWithFocusGuard(Supplier<Tab> launchSupplier) {
        Set<Activity> priorActivities =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new HashSet<>(ApplicationStatus.getRunningActivities()));
        AtomicBoolean active = new AtomicBoolean(true);
        Runnable focusTask =
                new Runnable() {
                    @Override
                    public void run() {
                        if (!active.get()) return;
                        for (Activity activity : ApplicationStatus.getRunningActivities()) {
                            if (priorActivities.contains(activity)) continue;
                            if (!(activity instanceof ChromeActivity)) continue;
                            if (activity.isFinishing() || activity.isDestroyed()) continue;
                            moveTaskToFrontIfPausedOrStopped(
                                    activity, ApplicationStatus.getStateForActivity(activity));
                        }
                        if (active.get()) {
                            ThreadUtils.postOnUiThreadDelayed(this, 100);
                        }
                    }
                };
        ApplicationStatus.ActivityStateListener stateListener =
                (activity, newState) -> {
                    if (!active.get()) return;
                    if (priorActivities.contains(activity)) return;
                    if (!(activity instanceof ChromeActivity)) return;
                    if (activity.isFinishing() || activity.isDestroyed()) return;
                    moveTaskToFrontIfPausedOrStopped(activity, newState);
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForAllActivities(stateListener);
                    ThreadUtils.postOnUiThreadDelayed(focusTask, 100);
                });
        try {
            return launchSupplier.get();
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        active.set(false);
                        ApplicationStatus.unregisterActivityStateListener(stateListener);
                    });
        }
    }

    private static Tab launchUrlInTab(
            FreshCtaTransitTestRule testRule, String url, boolean incognito) {
        return runWithFocusGuard(
                () -> {
                    List<Station<?>> stations = TrafficControl.getActiveStations();
                    if (stations.isEmpty()) {
                        WebPageStation blankStation =
                                incognito
                                        ? testRule.startOnIncognitoBlankPage()
                                        : testRule.startOnBlankPage();
                        return blankStation.loadWebPageProgrammatically(url).getTab();
                    }

                    WebPageStation activeStation = null;
                    for (int i = stations.size() - 1; i >= 0; i--) {
                        if (stations.get(i) instanceof WebPageStation) {
                            activeStation = (WebPageStation) stations.get(i);
                            break;
                        }
                    }
                    if (activeStation == null) {
                        throw new IllegalStateException("No active WebPageStation found.");
                    }

                    if (incognito) {
                        return activeStation
                                .openNewIncognitoTabOrWindowFast()
                                .loadWebPageProgrammatically(url)
                                .getTab();
                    } else {
                        return activeStation
                                .openNewTabOrWindowFast()
                                .loadWebPageProgrammatically(url)
                                .getTab();
                    }
                });
    }

    private static Tab launchUrlInCct(
            CustomTabActivityTestRule testRule, String url, boolean incognito) {
        return runWithFocusGuard(
                () -> {
                    Context context = ApplicationProvider.getApplicationContext();
                    Intent intent =
                            incognito
                                    ? createMinimalIncognitoCustomTabIntent(context, url)
                                    : createMinimalCustomTabIntent(context, url);

                    testRule.startCustomTabActivityWithIntent(intent);
                    Tab tab = testRule.getActivity().getActivityTab();

                    // Giving time to the WebContents to be ready.
                    CriteriaHelper.pollUiThread(
                            () ->
                                    Criteria.checkThat(
                                            tab.getWebContents(), Matchers.notNullValue()));

                    assertEquals(incognito, tab.getWebContents().isIncognito());
                    return tab;
                });
    }

    public static void closeTabs(FreshCtaTransitTestRule testRule) {
        ChromeActivity activity = testRule.getActivity();
        if (activity == null) return;
        activity.getTabModelSelector()
                .getModel(false)
                .getTabRemover()
                .closeTabs(TabClosureParams.closeAllTabs().build(), /* allowDialog= */ false);
        activity.getTabModelSelector()
                .getModel(true)
                .getTabRemover()
                .closeTabs(TabClosureParams.closeAllTabs().build(), /* allowDialog= */ false);
    }

    public static void tearDown(
            FreshCtaTransitTestRule chromeTabbedActivityRule,
            CustomTabActivityTestRule customTabActivityTestRule) {
        chromeTabbedActivityRule.closeAllWindowsAndDeleteInstanceAndTabState();
        CustomTabsTestUtils.cleanupSessions();
        CriteriaHelper.pollUiThread(() -> ApplicationStatus.getRunningActivities().isEmpty());
        customTabActivityTestRule.setActivity(null);
        chromeTabbedActivityRule.getActivityTestRule().setActivity(null);
    }

    // Warming up CCT so that the native is initialized before we access feature flags.
    public static void fireAndWaitForCctWarmup() throws TimeoutException {
        CustomTabsTestUtils.warmUpAndWait();
    }
}
