// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.metrics;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.app.Activity;
import android.app.SearchManager;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.view.KeyEvent;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.LauncherShortcutActivity;
import org.chromium.chrome.browser.ServiceTabLauncher;
import org.chromium.chrome.browser.ServiceTabLauncherJni;
import org.chromium.chrome.browser.bookmarkswidget.BookmarkWidgetProxy;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;

/** Integration tests for TabbedActivityLaunchCauseMetrics. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public final class TabbedActivityLaunchCauseMetricsTest {
    private static final long CHROME_LAUNCH_TIMEOUT = 10000L;

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule
    public static final ChromeBrowserTestRule sBrowserTestRule = new ChromeBrowserTestRule();

    @Rule public final JniMocker mJniMocker = new JniMocker();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ServiceTabLauncher.Natives mServiceTabLauncherJni;

    private static int histogramCountForValue(int value) {
        if (!LibraryLoader.getInstance().isInitialized()) return 0;
        return RecordHistogram.getHistogramValueCountForTesting(
                LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM, value);
    }

    @Test
    @MediumTest
    public void testMainIntentMetrics() throws Throwable {
        final int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.MAIN_LAUNCHER_ICON);
        mActivityTestRule.startMainActivityFromLauncher();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            histogramCountForValue(
                                    LaunchCauseMetrics.LaunchCause.MAIN_LAUNCHER_ICON),
                            Matchers.is(count + 1));
                },
                CHROME_LAUNCH_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ChromeApplicationTestUtils.fireHomeScreenIntent(mActivityTestRule.getActivity());
        mActivityTestRule.resumeMainActivityFromLauncher();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            histogramCountForValue(
                                    LaunchCauseMetrics.LaunchCause.MAIN_LAUNCHER_ICON),
                            Matchers.is(count + 2));
                });
    }

    @Test
    @MediumTest
    public void testNoMainIntentMetricsFromRecents() throws Throwable {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);

        ApplicationStatus.ActivityStateListener listener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(
                            Activity activity, @ActivityState int newState) {
                        if (newState == ActivityState.CREATED) {
                            // Android strips FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY from sent intents,
                            // so we have to inject it as early as possible during startup.
                            activity.setIntent(
                                    activity.getIntent()
                                            .addFlags(Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY));
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> ApplicationStatus.registerStateListenerForAllActivities(listener));

        final int mainCount =
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.MAIN_LAUNCHER_ICON);
        final int recentsCount = histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS);

        mActivityTestRule.startMainActivityFromIntent(intent, null);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS),
                            Matchers.is(recentsCount + 1));
                },
                CHROME_LAUNCH_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Assert.assertEquals(
                mainCount,
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.MAIN_LAUNCHER_ICON));
        ThreadUtils.runOnUiThreadBlocking(
                () -> ApplicationStatus.unregisterActivityStateListener(listener));
    }

    @Test
    @MediumTest
    public void testLauncherShortcutMetrics() throws Throwable {
        Intent intent = new Intent(LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB);
        intent.setClass(ContextUtils.getApplicationContext(), LauncherShortcutActivity.class);
        final int count =
                1
                        + histogramCountForValue(
                                LaunchCauseMetrics.LaunchCause.MAIN_LAUNCHER_ICON_SHORTCUT);
        mActivityTestRule.startMainActivityFromIntent(intent, null);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            histogramCountForValue(
                                    LaunchCauseMetrics.LaunchCause.MAIN_LAUNCHER_ICON_SHORTCUT),
                            Matchers.is(count));
                },
                CHROME_LAUNCH_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testBookmarkWidgetMetrics() throws Throwable {
        Intent intent = new Intent();
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(ContextUtils.getApplicationContext(), BookmarkWidgetProxy.class);
        intent.setData(Uri.parse("about:blank"));
        final int count =
                1 + histogramCountForValue(LaunchCauseMetrics.LaunchCause.HOME_SCREEN_WIDGET);
        mActivityTestRule.setActivity(
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () -> ContextUtils.getApplicationContext().startActivity(intent)));
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            histogramCountForValue(
                                    LaunchCauseMetrics.LaunchCause.HOME_SCREEN_WIDGET),
                            Matchers.is(count));
                },
                CHROME_LAUNCH_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private static class TestContext extends ContextWrapper {
        public TestContext(Context baseContext) {
            super(baseContext);
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
                    if (intent.getAction().equals(Intent.ACTION_WEB_SEARCH)) {
                        return Collections.emptyList();
                    }

                    return TestContext.super
                            .getPackageManager()
                            .queryIntentActivities(intent, flags);
                }
            };
        }
    }

    @Test
    @MediumTest
    @RequiresRestart("crbug.com/1223068")
    public void testExternalSearchIntentNoResolvers() throws Throwable {
        final int count =
                1
                        + histogramCountForValue(
                                LaunchCauseMetrics.LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT);
        final Context contextToRestore = ContextUtils.getApplicationContext();
        ContextUtils.initApplicationContextForTests(new TestContext(contextToRestore));

        Intent intent = new Intent(Intent.ACTION_SEARCH);
        intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        intent.putExtra(SearchManager.QUERY, "about:blank");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        SearchActivity searchActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        SearchActivity.class,
                        Stage.RESUMED,
                        () -> contextToRestore.startActivity(intent));

        onView(withId(R.id.url_bar)).perform(click());
        ChromeTabbedActivity cta =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        null,
                        () ->
                                onView(withId(R.id.url_bar))
                                        .perform(pressKey(KeyEvent.KEYCODE_ENTER)));

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            histogramCountForValue(
                                    LaunchCauseMetrics.LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT),
                            Matchers.is(count));
                },
                CHROME_LAUNCH_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ApplicationTestUtils.finishActivity(cta);
        ApplicationTestUtils.finishActivity(searchActivity);
        ContextUtils.initApplicationContextForTests(contextToRestore);
    }

    @Test
    @MediumTest
    public void testServiceWorkerTabLaunch() throws Throwable {
        final int count = 1 + histogramCountForValue(LaunchCauseMetrics.LaunchCause.NOTIFICATION);
        mJniMocker.mock(ServiceTabLauncherJni.TEST_HOOKS, mServiceTabLauncherJni);
        mActivityTestRule.setActivity(
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () -> {
                            ServiceTabLauncher.launchTab(
                                    0,
                                    false,
                                    new GURL("about:blank"),
                                    WindowOpenDisposition.NEW_FOREGROUND_TAB,
                                    "",
                                    ReferrerPolicy.DEFAULT,
                                    "",
                                    null);
                        }));
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            histogramCountForValue(LaunchCauseMetrics.LaunchCause.NOTIFICATION),
                            Matchers.is(count));
                },
                CHROME_LAUNCH_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
