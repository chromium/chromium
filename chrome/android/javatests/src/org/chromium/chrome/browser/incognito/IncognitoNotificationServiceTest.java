// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertTrue;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.content.Intent;
import android.service.notification.StatusBarNotification;
import android.util.Pair;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.TabStateInfo;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.LoadUrlParams;

import java.io.File;
import java.util.concurrent.Callable;

/** Tests for the Incognito Notification service. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoNotificationServiceTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    private void createTabOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable)
                        () ->
                                mActivityTestRule
                                        .getActivity()
                                        .getTabCreator(true)
                                        .createNewTab(
                                                new LoadUrlParams("about:blank"),
                                                TabLaunchType.FROM_CHROME_UI,
                                                null));
    }

    private void sendClearIncognitoIntent() throws CanceledException {
        PendingIntent clearIntent =
                IncognitoNotificationServiceImpl.getRemoveAllIncognitoTabsIntent(
                                ApplicationProvider.getApplicationContext())
                        .getPendingIntent();
        clearIntent.send();
    }

    private void launchIncognitoTabAndEnsureNotificationDisplayed() {
        mActivityTestRule.startMainActivityOnBlankPage();
        createTabOnUiThread();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule
                                    .getActivity()
                                    .getTabModelSelector()
                                    .getModel(true)
                                    .getCount(),
                            Matchers.greaterThanOrEqualTo(1));
                });

        Context context = ContextUtils.getApplicationContext();
        NotificationManager nm =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        boolean isIncognitoNotificationDisplayed = false;
        for (StatusBarNotification statusBarNotification : nm.getActiveNotifications()) {
            if (IncognitoNotificationManager.INCOGNITO_TABS_OPEN_TAG.equals(
                    statusBarNotification.getTag())) {
                isIncognitoNotificationDisplayed = true;
            }
        }
        assertTrue(isIncognitoNotificationDisplayed);
    }

    @Test
    @Feature("Incognito")
    @MediumTest
    public void testSingleRunningChromeTabbedActivity()
            throws InterruptedException, CanceledException {
        mActivityTestRule.startMainActivityOnBlankPage();

        createTabOnUiThread();
        createTabOnUiThread();

        pollUiThreadForChromeActivityIncognitoTabCount(2);

        final Profile incognitoProfile =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Profile>() {
                            @Override
                            public Profile call() {
                                return mActivityTestRule
                                        .getActivity()
                                        .getTabModelSelector()
                                        .getModel(true)
                                        .getProfile();
                            }
                        });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(incognitoProfile.isOffTheRecord());
                    assertTrue(incognitoProfile.isNativeInitialized());
                });

        sendClearIncognitoIntent();

        pollUiThreadForChromeActivityIncognitoTabCount(0);
        CriteriaHelper.pollUiThread(() -> !incognitoProfile.isNativeInitialized());
    }

    @Test
    @Feature("Incognito")
    @MediumTest
    @DisabledTest(message = "crbug.com/1033835")
    public void testNoAliveProcess() throws Exception {
        Context context = ApplicationProvider.getApplicationContext();
        final TestTabModelDirectory tabbedModeDirectory =
                new TestTabModelDirectory(context, "tabs", String.valueOf(0));

        // Add a couple non-incognito tabs (their filenames use a different prefix, so we do not
        // need to worry about ID space collisions with the generated incognito tabs).
        tabbedModeDirectory.writeTabStateFile(TestTabModelDirectory.V2_DUCK_DUCK_GO);
        tabbedModeDirectory.writeTabStateFile(TestTabModelDirectory.V2_BAIDU);

        // Generate a few incognito tabs (using arbitrary data from an existing TabState
        // definition).
        for (int i = 0; i < 3; i++) {
            TabStateInfo incognitoInfo =
                    new TabStateInfo(
                            true,
                            TestTabModelDirectory.V2_TEXTAREA.version,
                            i,
                            TestTabModelDirectory.V2_TEXTAREA.url,
                            TestTabModelDirectory.V2_TEXTAREA.title,
                            TestTabModelDirectory.V2_TEXTAREA.encodedTabState);
            tabbedModeDirectory.writeTabStateFile(incognitoInfo);
        }

        TabStateDirectory.setBaseStateDirectoryForTests(tabbedModeDirectory.getBaseDirectory());

        File[] tabbedModeFiles = tabbedModeDirectory.getDataDirectory().listFiles();
        Assert.assertNotNull(tabbedModeFiles);
        Assert.assertEquals(5, tabbedModeFiles.length);

        int incognitoCount = 0;
        int normalCount = 0;
        for (File tabbedModeFile : tabbedModeFiles) {
            Pair<Integer, Boolean> tabFileInfo =
                    TabStateFileManager.parseInfoFromFilename(tabbedModeFile.getName());
            if (tabFileInfo.second) incognitoCount++;
            else normalCount++;
        }
        Assert.assertEquals(2, normalCount);
        Assert.assertEquals(3, incognitoCount);

        sendClearIncognitoIntent();

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    File[] actualTabbedModeFiles =
                            tabbedModeDirectory.getDataDirectory().listFiles();
                    if (actualTabbedModeFiles == null) return;
                    int actualIncognitoCount = 0;
                    for (File tabbedModeFile : actualTabbedModeFiles) {
                        Pair<Integer, Boolean> tabFileInfo =
                                TabStateFileManager.parseInfoFromFilename(tabbedModeFile.getName());
                        if (tabFileInfo != null && tabFileInfo.second) actualIncognitoCount++;
                    }
                    Criteria.checkThat(actualIncognitoCount, Matchers.is(0));
                });

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    File[] actualTabbedModeFiles =
                            tabbedModeDirectory.getDataDirectory().listFiles();
                    Criteria.checkThat(actualTabbedModeFiles, Matchers.notNullValue());
                    int actualNormalCount = 0;
                    for (File tabbedModeFile : actualTabbedModeFiles) {
                        Pair<Integer, Boolean> tabFileInfo =
                                TabStateFileManager.parseInfoFromFilename(tabbedModeFile.getName());
                        if (tabFileInfo != null && !tabFileInfo.second) actualNormalCount++;
                    }
                    Criteria.checkThat(actualNormalCount, Matchers.is(2));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(LibraryLoader.getInstance().isInitialized()));
    }

    @Test
    @MediumTest
    @Feature("Incognito")
    public void testCloseAllIncognitoNotificationIsDisplayed() {
        launchIncognitoTabAndEnsureNotificationDisplayed();
    }

    @Test
    @MediumTest
    @Feature("Incognito")
    public void testCloseAllIncognitoNotificationForIncognitoCCT_DoesNotCloseCCT()
            throws PendingIntent.CanceledException {
        launchIncognitoTabAndEnsureNotificationDisplayed();

        // Create an Incognito CCT now.
        Intent customTabIntent =
                CustomTabsIntentTestUtils.createMinimalIncognitoCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), "about:blank");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        // Click on "Close all Incognito tabs" notification.
        PendingIntent clearIntent =
                IncognitoNotificationServiceImpl.getRemoveAllIncognitoTabsIntent(
                                ApplicationProvider.getApplicationContext())
                        .getPendingIntent();
        clearIntent.send();

        pollUiThreadForChromeActivityIncognitoTabCount(0);

        // Verify the Incognito CCT is not closed.
        pollUiThreadForCustomIncognitoTabCount(1);
    }

    private void pollUiThreadForChromeActivityIncognitoTabCount(int expectedCount) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule
                                    .getActivity()
                                    .getTabModelSelector()
                                    .getModel(true)
                                    .getCount(),
                            Matchers.is(expectedCount));
                });
    }

    private void pollUiThreadForCustomIncognitoTabCount(int expectedCount) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mCustomTabActivityTestRule
                                    .getActivity()
                                    .getTabModelSelector()
                                    .getModel(true)
                                    .getCount(),
                            Matchers.is(expectedCount));
                });
    }
}
