// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.stream.Collectors;

/** Tests for Android NMR1 launcher shortcuts. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "This class tests activity start behavior and thus cannot be batched.")
public class LauncherShortcutTest {

    /** Used for parameterized tests to toggle whether an incognito or regular tab is created. */
    public static class IncognitoParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(false).name("RegularTab"),
                    new ParameterSet().value(true).name("IncognitoTab"));
        }
    }

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private TabModelSelector mTabModelSelector;
    private final CallbackHelper mTabAddedCallback = new CallbackHelper();

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelSelectorObserver tabModelSelectorObserver =
                            new TabModelSelectorObserver() {
                                @Override
                                public void onNewTabCreated(
                                        Tab tab, @TabCreationState int creationState) {
                                    mTabAddedCallback.notifyCalled();
                                }
                            };
                    mTabModelSelector.addObserver(tabModelSelectorObserver);
                });
    }

    @After
    public void tearDown() {
        ShortcutManager shortcutManager =
                mActivityTestRule.getActivity().getSystemService(ShortcutManager.class);
        List<String> idsToRemove = new ArrayList<>();
        idsToRemove.add(LauncherShortcutActivity.DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID);
        idsToRemove.add(LauncherShortcutActivity.DYNAMIC_OPEN_NEW_WINDOW_ID);
        shortcutManager.disableShortcuts(idsToRemove);
        shortcutManager.removeDynamicShortcuts(idsToRemove);

        List<ShortcutInfo> remainingShortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals(
                "Dynamic shortcuts should be cleared in setUp", 0, remainingShortcuts.size());
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(IncognitoParams.class)
    public void testLauncherShortcut(boolean incognito) throws Exception {
        int initialTabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> mTabModelSelector.getTotalTabCount());

        Intent intent =
                new Intent(
                        incognito
                                ? (IncognitoUtils.shouldOpenIncognitoAsWindow()
                                        ? LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_WINDOW
                                        : LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB)
                                : LauncherShortcutActivity.ACTION_OPEN_NEW_TAB);
        intent.setClass(mActivityTestRule.getActivity(), LauncherShortcutActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
        if (incognito && IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            List<Activity> runningActivities = ApplicationStatus.getRunningActivities();
            Assert.assertEquals(
                    "Incorrect number of running activities.", 2, runningActivities.size());
            Assert.assertTrue(
                    "Running activities should contain a regular ChromeTabbedActivity.",
                    runningActivities.stream()
                            .anyMatch(
                                    activity ->
                                            activity instanceof ChromeTabbedActivity cta
                                                    && !cta.isIncognitoWindow()));
            Assert.assertTrue(
                    "Running activities should contain an incognito ChromeTabbedActivity.",
                    runningActivities.stream()
                            .anyMatch(
                                    activity ->
                                            activity instanceof ChromeTabbedActivity cta
                                                    && cta.isIncognitoWindow()));
        } else {
        mTabAddedCallback.waitForCallback(0);
        // Verify NTP was created.
        Tab activityTab = mActivityTestRule.getActivityTab();
        Assert.assertEquals(
                "Incorrect tab launch type.",
                TabLaunchType.FROM_LAUNCHER_SHORTCUT,
                activityTab.getLaunchType());

        Assert.assertTrue(
                "Tab should be an NTP. Tab url: " + ChromeTabUtils.getUrlOnUiThread(activityTab),
                UrlUtilities.isNtpUrl(ChromeTabUtils.getUrlOnUiThread(activityTab)));

        // Verify tab model.
        Assert.assertEquals(
                "Incorrect tab model selected.",
                incognito,
                mTabModelSelector.isIncognitoSelected());
        int tabCount = ThreadUtils.runOnUiThreadBlocking(() -> mTabModelSelector.getTotalTabCount());
        Assert.assertEquals("Incorrect total tab count.", initialTabCount + 1, tabCount);
        Assert.assertEquals(
                "Incorrect normal tab count.",
                incognito ? initialTabCount : initialTabCount + 1,
                mActivityTestRule.tabsCount(false));
        Assert.assertEquals(
                "Incorrect incognito tab count.",
                incognito ? 1 : 0,
                mActivityTestRule.tabsCount(true));
        }
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(IncognitoParams.class)
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testLauncherShortcut_SplitScreenLaunch(boolean incognito) throws Exception {
        Intent intent =
                new Intent(
                        incognito
                                ? LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB
                                : LauncherShortcutActivity.ACTION_OPEN_NEW_TAB);
        intent.setClass(mActivityTestRule.getActivity(), LauncherShortcutActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // The intent originating from the task bar shortcut drag to split screen is expected to
        // contain FLAG_ACTIVITY_MULTIPLE_TASK.
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);

        ChromeTabbedActivity newWindowActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> ContextUtils.getApplicationContext().startActivity(intent));

        // Verify that a new Chrome instance was created.
        Assert.assertEquals(
                "Number of Chrome instances should be correct.",
                2,
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY));

        // Verify NTP was created in the new activity.
        CriteriaHelper.pollUiThread(() -> newWindowActivity.getActivityTab() != null);
        Tab activityTab = ThreadUtils.runOnUiThreadBlocking(newWindowActivity::getActivityTab);
        Assert.assertEquals(
                "Incorrect tab launch type.",
                TabLaunchType.FROM_LAUNCHER_SHORTCUT,
                activityTab.getLaunchType());

        Assert.assertTrue(
                "Tab should have the NTP tab url: " + ChromeTabUtils.getUrlOnUiThread(activityTab),
                UrlUtilities.isNtpUrl(ChromeTabUtils.getUrlOnUiThread(activityTab)));

        // Verify the tab model.
        Assert.assertEquals(
                "Incorrect tab model selected.",
                incognito,
                newWindowActivity.getTabModelSelector().isIncognitoSelected());
    }

    @Test(expected = TimeoutException.class)
    @MediumTest
    public void testInvalidIntent() throws TimeoutException {
        Intent intent = new Intent("fooAction");
        intent.setClass(mActivityTestRule.getActivity(), LauncherShortcutActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        mTabAddedCallback.waitForCallback(0);
    }

    @Test
    @MediumTest
    public void testManifestShortcuts() {
        ShortcutManager shortcutManager =
                mActivityTestRule.getActivity().getSystemService(ShortcutManager.class);
        List<ShortcutInfo> shortcuts = shortcutManager.getManifestShortcuts();
        Assert.assertEquals("Incorrect number of manifest shortcuts.", 1, shortcuts.size());
        Assert.assertEquals(
                "Incorrect manifest shortcut id.", "new-tab-shortcut", shortcuts.get(0).getId());
    }

    private void testDynamicShortcutsInternal() {
        List<String> expectedLabels;
        int expectedSize;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            expectedLabels = Arrays.asList("New window", "New Incognito window");
            expectedSize = 2;
        } else {
            expectedLabels = Arrays.asList("New Incognito tab");
            expectedSize = 1;
        }

        IncognitoUtils.setEnabledForTesting(true);
        LauncherShortcutActivity.updateIncognitoShortcut(
                mActivityTestRule.getActivity(), mActivityTestRule.getProfile(false));
        ShortcutManager shortcutManager =
                mActivityTestRule.getActivity().getSystemService(ShortcutManager.class);
        List<ShortcutInfo> shortcuts = shortcutManager.getDynamicShortcuts();
        List<String> actualLabels =
                shortcuts.stream()
                        .map(shortcut -> shortcut.getLongLabel().toString())
                        .collect(Collectors.toList());

        Assert.assertEquals(
                "The number of shortcuts was incorrect.", expectedSize, actualLabels.size());
        Assert.assertTrue(
                "The list did not contain all expected labels.",
                actualLabels.containsAll(expectedLabels));

        IncognitoUtils.setEnabledForTesting(false);
        LauncherShortcutActivity.updateIncognitoShortcut(
                mActivityTestRule.getActivity(), mActivityTestRule.getProfile(false));
        shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals("Incorrect number of dynamic shortcuts.", 0, shortcuts.size());

        IncognitoUtils.setEnabledForTesting(true);
        LauncherShortcutActivity.updateIncognitoShortcut(
                mActivityTestRule.getActivity(), mActivityTestRule.getProfile(false));
        shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals(
                "Incorrect number of dynamic shortcuts after re-enabling incognito.",
                expectedSize,
                shortcuts.size());
    }

    @Test
    @SmallTest
    public void testDynamicShortcuts() {
        testDynamicShortcutsInternal();
    }

    private void testDynamicShortcuts_LanguageChangeInternal() {
        IncognitoUtils.setEnabledForTesting(true);
        LauncherShortcutActivity.updateIncognitoShortcut(
                mActivityTestRule.getActivity(), mActivityTestRule.getProfile(false));

        List<String> expectedLabels;
        int expectedSize;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            expectedLabels = Arrays.asList("New window", "New Incognito window");
            expectedSize = 2;
        } else {
            expectedLabels = Arrays.asList("New Incognito tab");
            expectedSize = 1;
        }

        ShortcutManager shortcutManager =
                mActivityTestRule.getActivity().getSystemService(ShortcutManager.class);
        List<ShortcutInfo> shortcuts = shortcutManager.getDynamicShortcuts();
        List<String> actualLabels =
                shortcuts.stream()
                        .map(shortcut -> shortcut.getLongLabel().toString())
                        .collect(Collectors.toList());

        Assert.assertEquals(
                "The number of shortcuts was incorrect.", expectedSize, actualLabels.size());
        Assert.assertTrue(
                "The list did not contain all expected labels.",
                actualLabels.containsAll(expectedLabels));

        LauncherShortcutActivity.setDynamicShortcutStringForTesting("Foo");
        LauncherShortcutActivity.updateIncognitoShortcut(
                mActivityTestRule.getActivity(), mActivityTestRule.getProfile(false));
        shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals(
                "Incorrect number of dynamic shortcuts after updating.",
                expectedSize,
                shortcuts.size());

        Assert.assertEquals(
                "Incorrect label after updating.", "Foo", shortcuts.get(0).getLongLabel());
    }

    @Test
    @SmallTest
    public void testDynamicShortcuts_LanguageChange() {
        testDynamicShortcuts_LanguageChangeInternal();
    }
}
