// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.os.Build;
import android.os.Build.VERSION_CODES;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

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
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for Android NMR1 launcher shortcuts. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RequiresApi(VERSION_CODES.N_MR1)
@MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
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
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private TabModelSelector mTabModelSelector;
    private CallbackHelper mTabAddedCallback = new CallbackHelper();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
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

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(IncognitoParams.class)
    public void testLauncherShortcut(boolean incognito) throws Exception {
        int initialTabCount = mTabModelSelector.getTotalTabCount();

        Intent intent =
                new Intent(
                        incognito
                                ? LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB
                                : LauncherShortcutActivity.ACTION_OPEN_NEW_TAB);
        intent.setClass(mActivityTestRule.getActivity(), LauncherShortcutActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        mTabAddedCallback.waitForCallback(0);

        // Verify NTP was created.

        Tab activityTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getActivity().getActivityTab());
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
        Assert.assertEquals(
                "Incorrect total tab count.",
                initialTabCount + 1,
                mTabModelSelector.getTotalTabCount());
        Assert.assertEquals(
                "Incorrect normal tab count.",
                incognito ? initialTabCount : initialTabCount + 1,
                mTabModelSelector.getModel(false).getCount());
        Assert.assertEquals(
                "Incorrect incognito tab count.",
                incognito ? 1 : 0,
                mTabModelSelector.getModel(true).getCount());
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
                MultiWindowUtils.getInstanceCount());

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

    @Test
    @SmallTest
    public void testDynamicShortcuts() {
        IncognitoUtils.setEnabledForTesting(true);
        LauncherShortcutActivity.updateIncognitoShortcut(
                mActivityTestRule.getActivity(), mActivityTestRule.getProfile(false));
        ShortcutManager shortcutManager =
                mActivityTestRule.getActivity().getSystemService(ShortcutManager.class);
        List<ShortcutInfo> shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals("Incorrect number of dynamic shortcuts.", 1, shortcuts.size());
        Assert.assertEquals(
                "Incorrect dynamic shortcut id.",
                LauncherShortcutActivity.DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID,
                shortcuts.get(0).getId());

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
                1,
                shortcuts.size());
    }

    @Test
    @SmallTest
    public void testDynamicShortcuts_LanguageChange() {
        IncognitoUtils.setEnabledForTesting(true);
        LauncherShortcutActivity.updateIncognitoShortcut(
                mActivityTestRule.getActivity(), mActivityTestRule.getProfile(false));
        ShortcutManager shortcutManager =
                mActivityTestRule.getActivity().getSystemService(ShortcutManager.class);
        List<ShortcutInfo> shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals("Incorrect number of dynamic shortcuts.", 1, shortcuts.size());
        Assert.assertEquals(
                "Incorrect label", "New Incognito tab", shortcuts.get(0).getLongLabel());

        LauncherShortcutActivity.setDynamicShortcutStringForTesting("Foo");
        LauncherShortcutActivity.updateIncognitoShortcut(
                mActivityTestRule.getActivity(), mActivityTestRule.getProfile(false));
        shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals(
                "Incorrect number of dynamic shortcuts after updating.", 1, shortcuts.size());
        Assert.assertEquals(
                "Incorrect label after updating.", "Foo", shortcuts.get(0).getLongLabel());
    }
}
