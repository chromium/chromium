// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.hamcrest.CoreMatchers.notNullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import android.content.Context;
import android.content.Intent;
import android.os.Build;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroid;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroidJni;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTrackerFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.List;

/** Tests for moving tabs between windows. These APIs are only available on desktop android. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@DoNotBatch(reason = "Multi-instance tests do not work with batching.")
@MinAndroidSdkLevel(Build.VERSION_CODES.S)
@Restriction(DeviceFormFactor.DESKTOP)
@ImportantFormFactors({DeviceFormFactor.DESKTOP})
public class TabModelMultiWindowTest {

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private MediaCaptureDevicesDispatcherAndroid.Natives mMediaCaptureDevicesDispatcherAndroidJni;

    private WebPageStation mPage;
    private TabModelJniBridge mTabModelJni;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabModelJni =
                (TabModelJniBridge)
                        mActivityTestRule.getActivity().getTabModelSelector().getModel(false);

        // Disabling media capture to avoid flakes.
        MediaCaptureDevicesDispatcherAndroidJni.setInstanceForTesting(
                mMediaCaptureDevicesDispatcherAndroidJni);
        when(mMediaCaptureDevicesDispatcherAndroidJni.isCapturingAudio(any())).thenReturn(false);
    }

    @Test
    @LargeTest
    public void testMoveTabToWindow() {
        ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();
        Tab tabToMove = createTab();
        int initialTabCount = getTabCountOnUiThread(mTabModelJni);

        // Create a new window.
        ChromeTabbedActivity activity2 = createNewWindow(activity1);
        assertNotNull(activity2);
        assertNotEquals(activity1, activity2);

        long nativeBrowserWindow2 = getNativeBrowserWindow(activity2);

        runOnUiThreadBlocking(
                () -> mTabModelJni.moveTabToWindowForTesting(tabToMove, nativeBrowserWindow2, 0));

        assertEquals(initialTabCount - 1, getTabCountOnUiThread(mTabModelJni));
        TabModel model2 = activity2.getTabModelSelector().getModel(false);
        assertEquals(2, getTabCountOnUiThread(model2));
        assertEquals(tabToMove, runOnUiThreadBlocking(() -> model2.getTabAt(0)));
    }

    @Test
    @LargeTest
    public void testMoveTabGroupToWindow() {
        ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();
        TabGroupModelFilter filter =
                activity1
                        .getTabModelSelector()
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(false);
        List<Tab> group = createTabGroup(2, filter);
        Token groupId = group.get(0).getTabGroupId();
        int initialTabCount = getTabCountOnUiThread(mTabModelJni);

        // Create a new window.
        ChromeTabbedActivity activity2 = createNewWindow(activity1);
        assertNotNull(activity2);
        assertNotEquals(activity1, activity2);

        long nativeBrowserWindow2 = getNativeBrowserWindow(activity2);

        runOnUiThreadBlocking(
                () ->
                        mTabModelJni.moveTabGroupToWindowForTesting(
                                groupId, nativeBrowserWindow2, 0));

        assertEquals(initialTabCount - 2, getTabCountOnUiThread(mTabModelJni));
        TabModel model2 = activity2.getTabModelSelector().getModel(false);
        assertEquals(3, getTabCountOnUiThread(model2));
        assertEquals(group.get(0), runOnUiThreadBlocking(() -> model2.getTabAt(0)));
        assertEquals(group.get(1), runOnUiThreadBlocking(() -> model2.getTabAt(1)));
        assertEquals(groupId, group.get(0).getTabGroupId());
        assertEquals(groupId, group.get(1).getTabGroupId());
    }

    private long getNativeBrowserWindow(ChromeTabbedActivity activity) {
        return runOnUiThreadBlocking(
                () -> {
                    var taskTracker = ChromeAndroidTaskTrackerFactory.getInstance();
                    var chromeTask = taskTracker.get(activity.getTaskId());
                    return chromeTask.getOrCreateNativeBrowserWindowPtr();
                });
    }

    private Tab createTab() {
        return ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                UrlConstants.ABOUT_URL,
                /* incognito= */ false);
    }

    private List<Tab> createTabGroup(int numberOfTabs, TabGroupModelFilter filter) {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < numberOfTabs; i++) tabs.add(createTab());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        filter.mergeListOfTabsToGroup(
                                tabs,
                                tabs.get(0),
                                /* notify= */ TabGroupModelFilter.MergeNotificationType
                                        .DONT_NOTIFY));
        return tabs;
    }

    private ChromeTabbedActivity createNewWindow(Context context) {
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        context,
                        /* instanceId= */ -1,
                        /* preferNew= */ true,
                        /* openAdjacently= */ false,
                        /* addTrustedIntentExtras= */ true,
                        NewWindowAppSource.OTHER);
        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () -> ContextUtils.getApplicationContext().startActivity(intent));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                "Activity tab should be non-null.",
                                activity.getActivityTab(),
                                notNullValue()));
        Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> activity.getActivityTab());
        ChromeTabUtils.loadUrlOnUiThread(tab, UrlConstants.GOOGLE_URL);
        return activity;
    }
}
