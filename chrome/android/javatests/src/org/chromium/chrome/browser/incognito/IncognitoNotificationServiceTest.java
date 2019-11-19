// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.util.Pair;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.TabStateInfo;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.util.concurrent.Callable;

/**
 * Tests for the Incognito Notification service.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoNotificationServiceTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private void createTabOnUiThread() {
        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> mActivityTestRule.getActivity().getTabCreator(true).createNewTab(
                                new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI,
                                null));
    }

    private void sendClearIncognitoIntent() throws CanceledException {
        PendingIntent clearIntent =
                IncognitoNotificationService
                        .getRemoveAllIncognitoTabsIntent(InstrumentationRegistry.getTargetContext())
                        .getPendingIntent();
        clearIntent.send();
    }

    @Test
    @Feature("Incognito")
    @MediumTest
    public void testSingleRunningChromeTabbedActivity()
            throws InterruptedException, CanceledException {
        mActivityTestRule.startMainActivityOnBlankPage();

        createTabOnUiThread();
        createTabOnUiThread();

        CriteriaHelper.pollUiThread(Criteria.equals(2, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getModel(true)
                        .getCount();
            }
        }));

        final Profile incognitoProfile =
                TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Profile>() {
                    @Override
                    public Profile call() {
                        return mActivityTestRule.getActivity()
                                .getTabModelSelector()
                                .getModel(true)
                                .getProfile();
                    }
                });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(incognitoProfile.isOffTheRecord());
            Assert.assertTrue(incognitoProfile.isNativeInitialized());
        });

        sendClearIncognitoIntent();

        CriteriaHelper.pollUiThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getModel(true)
                        .getCount();
            }
        }));
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !incognitoProfile.isNativeInitialized();
            }
        });
    }

    @Test
    @Feature("Incognito")
    @MediumTest
    @RetryOnFailure
    public void testNoAliveProcess() throws Exception {
        Context context = InstrumentationRegistry.getTargetContext();
        final TestTabModelDirectory tabbedModeDirectory = new TestTabModelDirectory(
                context, "tabs", String.valueOf(0));

        // Add a couple non-incognito tabs (their filenames use a different prefix, so we do not
        // need to worry about ID space collisions with the generated incognito tabs).
        tabbedModeDirectory.writeTabStateFile(TestTabModelDirectory.V2_DUCK_DUCK_GO);
        tabbedModeDirectory.writeTabStateFile(TestTabModelDirectory.V2_BAIDU);

        // Generate a few incognito tabs (using arbitrary data from an existing TabState
        // definition).
        for (int i = 0; i < 3; i++) {
            TabStateInfo incognitoInfo = new TabStateInfo(
                    true,
                    TestTabModelDirectory.V2_TEXTAREA.version,
                    i,
                    TestTabModelDirectory.V2_TEXTAREA.url,
                    TestTabModelDirectory.V2_TEXTAREA.title,
                    TestTabModelDirectory.V2_TEXTAREA.encodedTabState);
            tabbedModeDirectory.writeTabStateFile(incognitoInfo);
        }

        TabPersistentStore.setBaseStateDirectoryForTests(tabbedModeDirectory.getBaseDirectory());

        File[] tabbedModeFiles = tabbedModeDirectory.getDataDirectory().listFiles();
        Assert.assertNotNull(tabbedModeFiles);
        Assert.assertEquals(5, tabbedModeFiles.length);

        int incognitoCount = 0;
        int normalCount = 0;
        for (File tabbedModeFile : tabbedModeFiles) {
            Pair<Integer, Boolean> tabFileInfo =
                    TabState.parseInfoFromFilename(tabbedModeFile.getName());
            if (tabFileInfo.second) incognitoCount++;
            else normalCount++;
        }
        Assert.assertEquals(2, normalCount);
        Assert.assertEquals(3, incognitoCount);

        sendClearIncognitoIntent();

        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                File[] tabbedModeFiles = tabbedModeDirectory.getDataDirectory().listFiles();
                if (tabbedModeFiles == null) return 0;
                int incognitoCount = 0;
                for (File tabbedModeFile : tabbedModeFiles) {
                    Pair<Integer, Boolean> tabFileInfo =
                            TabState.parseInfoFromFilename(tabbedModeFile.getName());
                    if (tabFileInfo != null && tabFileInfo.second) incognitoCount++;
                }
                return incognitoCount;
            }
        }));

        CriteriaHelper.pollInstrumentationThread(Criteria.equals(2, new Callable<Integer>() {
            @Override
            public Integer call() {
                File[] tabbedModeFiles = tabbedModeDirectory.getDataDirectory().listFiles();
                if (tabbedModeFiles == null) return 0;
                int normalCount = 0;
                for (File tabbedModeFile : tabbedModeFiles) {
                    Pair<Integer, Boolean> tabFileInfo =
                            TabState.parseInfoFromFilename(tabbedModeFile.getName());
                    if (tabFileInfo != null && !tabFileInfo.second) normalCount++;
                }
                return normalCount;
            }
        }));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(LibraryLoader.getInstance().isInitialized()));
    }
}
