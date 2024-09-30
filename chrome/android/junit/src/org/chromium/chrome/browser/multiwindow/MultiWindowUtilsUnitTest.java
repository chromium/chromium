// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.HISTOGRAM_DESKTOP_WINDOW_COUNT_EXISTING_INSTANCE_SUFFIX;
import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.HISTOGRAM_DESKTOP_WINDOW_COUNT_NEW_INSTANCE_SUFFIX;
import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW;
import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW;

import android.app.Activity;
import android.content.Context;
import android.os.Build.VERSION_CODES;
import android.util.SparseIntArray;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils.InstanceAllocationType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtilsUnitTest.ShadowMultiInstanceManagerApi31;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

/** Unit tests for {@link MultiWindowUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowMultiInstanceManagerApi31.class})
public class MultiWindowUtilsUnitTest {
    /** Shadows {@link MultiInstanceManagerApi31} class for testing. */
    @Implements(MultiInstanceManagerApi31.class)
    public static class ShadowMultiInstanceManagerApi31 {
        private static SparseIntArray sWindowIdsOfRunningTabbedActivities;
        private static int sRunningTabbedActivityCount;

        public static void updateWindowIdsOfRunningTabbedActivities(int windowId, boolean remove) {
            if (sWindowIdsOfRunningTabbedActivities == null) {
                sWindowIdsOfRunningTabbedActivities = new SparseIntArray();
            }
            if (!remove) {
                sWindowIdsOfRunningTabbedActivities.put(windowId, windowId);
            } else {
                sWindowIdsOfRunningTabbedActivities.delete(windowId);
            }
        }

        public static void updateRunningTabbedActivityCount(int count) {
            sRunningTabbedActivityCount = count;
        }

        public static void reset() {
            sWindowIdsOfRunningTabbedActivities = null;
            sRunningTabbedActivityCount = 0;
        }

        @Implementation
        public static SparseIntArray getWindowIdsOfRunningTabbedActivities() {
            return sWindowIdsOfRunningTabbedActivities;
        }

        @Implementation
        public static int getRunningTabbedActivityCount() {
            return sRunningTabbedActivityCount;
        }
    }

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    private static final int INSTANCE_ID_0 = 0;
    private static final int INSTANCE_ID_1 = 1;
    private static final int INSTANCE_ID_2 = 2;
    private static final int TASK_ID_5 = 5;
    private static final int TASK_ID_6 = 6;
    private static final int TASK_ID_7 = 7;
    private static final String URL_1 = "url1";
    private static final String URL_2 = "url2";
    private static final String URL_3 = "url3";
    private static final GURL NTP_GURL = new GURL(UrlConstants.NTP_URL);
    private static final GURL TEST_GURL = new GURL("https://youtube.com/");

    private MultiWindowUtils mUtils;
    private boolean mIsInMultiWindowMode;
    private boolean mIsInMultiDisplayMode;
    private boolean mIsMultipleInstanceRunning;
    private boolean mIsAutosplitSupported;
    private boolean mCustomMultiWindowSupported;
    private Boolean mOverrideOpenInNewWindowSupported;

    @Mock TabModelSelector mTabModelSelector;
    @Mock TabModel mNormalTabModel;
    @Mock TabModel mIncognitoTabModel;
    @Mock HomepageManager mHomepageManager;
    @Mock DesktopWindowStateProvider mDesktopWindowStateProvider;
    @Mock AppHeaderState mAppHeaderState;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mUtils =
                new MultiWindowUtils() {
                    @Override
                    public boolean isInMultiWindowMode(Activity activity) {
                        return mIsInMultiWindowMode;
                    }

                    @Override
                    public boolean isInMultiDisplayMode(Activity activity) {
                        return mIsInMultiDisplayMode;
                    }

                    @Override
                    public boolean areMultipleChromeInstancesRunning(Context context) {
                        return mIsMultipleInstanceRunning;
                    }

                    @Override
                    public boolean aospMultiWindowModeSupported() {
                        return mIsAutosplitSupported;
                    }

                    @Override
                    public boolean customMultiWindowModeSupported() {
                        return mCustomMultiWindowSupported;
                    }

                    @Override
                    public Class<? extends Activity> getOpenInOtherWindowActivity(
                            Activity current) {
                        return Activity.class;
                    }

                    @Override
                    public boolean isOpenInOtherWindowSupported(Activity activity) {
                        if (mOverrideOpenInNewWindowSupported != null) {
                            return mOverrideOpenInNewWindowSupported;
                        }
                        return super.isOpenInOtherWindowSupported(activity);
                    }
                };

        when(mHomepageManager.isHomepageEnabled()).thenReturn(true);
        when(mHomepageManager.getHomepageGurl()).thenReturn(NTP_GURL);
        HomepageManager.setInstanceForTesting(mHomepageManager);

        when(mDesktopWindowStateProvider.getAppHeaderState()).thenReturn(mAppHeaderState);
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);
    }

    @After
    public void tearDown() {
        ShadowMultiInstanceManagerApi31.reset();
        mOverrideOpenInNewWindowSupported = false;
    }

    @Test
    public void testCanEnterMultiWindowMode() {
        // Chrome can enter multi-window mode through menu on the platform that supports it
        // (Android S or certain vendor-customized platform).
        for (int i = 0; i < 32; ++i) {
            mIsInMultiWindowMode = ((i >> 0) & 1) == 1;
            mIsInMultiDisplayMode = ((i >> 1) & 1) == 1;
            mIsMultipleInstanceRunning = ((i >> 2) & 1) == 1;
            mIsAutosplitSupported = ((i >> 3) & 1) == 1;
            mCustomMultiWindowSupported = ((i >> 4) & 1) == 1;

            boolean canEnter = mIsAutosplitSupported || mCustomMultiWindowSupported;
            assertEquals(
                    " api-s: " + mIsAutosplitSupported + " vendor: " + mCustomMultiWindowSupported,
                    canEnter,
                    mUtils.canEnterMultiWindowMode(null));
        }
    }

    @Test
    public void testIsOpenInOtherWindowEnabled() {
        for (int i = 0; i < 32; ++i) {
            mIsInMultiWindowMode = ((i >> 0) & 1) == 1;
            mIsInMultiDisplayMode = ((i >> 1) & 1) == 1;
            mIsMultipleInstanceRunning = ((i >> 2) & 1) == 1;
            mIsAutosplitSupported = ((i >> 3) & 1) == 1;
            mCustomMultiWindowSupported = ((i >> 4) & 1) == 1;

            // 'openInOtherWindow' is supported if we are already in multi-window/display mode.
            boolean openInOtherWindow = (mIsInMultiWindowMode || mIsInMultiDisplayMode);
            assertEquals(
                    "multi-window: "
                            + mIsInMultiWindowMode
                            + " multi-display: "
                            + mIsInMultiDisplayMode
                            + " multi-instance: "
                            + mIsMultipleInstanceRunning,
                    openInOtherWindow,
                    mUtils.isOpenInOtherWindowSupported(null));
        }
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    public void testIsMoveOtherWindowSupported_InstanceSwitcherEnabled_ReturnsTrue() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);

        // Instance with no tabs (ID_1) still counts as long as it is alive.
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 0, /* incognitoTabCount= */ 0, TASK_ID_6);

        assertTrue(
                "Should return true on Android R+ with multiple tabs.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    @Config(sdk = VERSION_CODES.Q)
    public void
            testIsMoveOtherWindowSupported_InstanceSwitcherDisabledAndOpenInOtherWindowAllowed_ReturnsTrue() {
        mOverrideOpenInNewWindowSupported = true;
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        assertTrue(
                "Should return true on Android Q with multiple tabs.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    public void testIsMoveOtherWindowSupported_HasOneTabWithHomePageDisabled_ReturnsTrue() {
        when(mHomepageManager.isHomepageEnabled()).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        mOverrideOpenInNewWindowSupported = true;
        assertTrue(
                "Should return true when called for last tab with homepage disabled.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    public void testIsMoveOtherWindowSupported_HasOneTabWithHomePageEnabledAsNtp_ReturnsTrue() {
        mOverrideOpenInNewWindowSupported = true;
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        assertTrue(
                "Should return true when called for last tab with homepage enabled as NTP.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    public void
            testIsMoveOtherWindowSupported_HasOneTabWithHomePageEnabledAsCustomUrl_ReturnsFalse() {
        when(mHomepageManager.getHomepageGurl()).thenReturn(TEST_GURL);
        when(mHomepageManager.isHomepageEnabled()).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        assertFalse(
                "Should return false when called for last tab with homepage set as a custom url.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    public void testIsMoveOtherWindowSupported_OnAutomotive_ReturnsFalse() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        assertFalse(
                "Should return false for automotive.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabled_OneTab_HasCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        assertTrue(
                "Should return true with one tab and custom homepage.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabled_OneTab_NoCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        assertFalse(
                "Should return false with one tab and no custom homepage.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabled_WithMoreThanOneTab_HasCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        assertFalse(
                "Should return false for multiple tabs.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabled_WithMoreThanOneTab_NoCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        assertFalse(
                "Should return false for multiple tabs.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void testGetInstanceCount() {
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        // Instance with no tabs (ID_1) still counts as long as it is alive.
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 0, /* incognitoTabCount= */ 0, TASK_ID_6);
        writeInstanceInfo(
                INSTANCE_ID_2, URL_3, /* tabCount= */ 6, /* incognitoTabCount= */ 2, TASK_ID_7);
        assertEquals(3, MultiWindowUtils.getInstanceCount());

        // Instance with no running task is not taken into account if there is no normal tab,
        // regardless of the # of incognito tabs.
        writeInstanceInfo(
                INSTANCE_ID_1,
                URL_2,
                /* tabCount= */ 0,
                /* incognitoTabCount= */ 2,
                MultiWindowUtils.INVALID_TASK_ID);
        assertEquals(2, MultiWindowUtils.getInstanceCount());
    }

    @Test
    @Config(sdk = 31)
    public void testGetInstanceIdForViewIntent_LessThanMaxInstancesOpen() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        int maxInstances = MultiWindowUtils.getMaxInstances();
        // Simulate opening of 1 less than the max number of instances. #writeInstanceInfo will
        // update the access time for IDs 0 -> |maxInstances - 2| in increasing order of recency.
        for (int i = 0; i < maxInstances - 1; i++) {
            ShadowMultiInstanceManagerApi31.updateWindowIdsOfRunningTabbedActivities(i, false);
            writeInstanceInfo(i, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, i);
        }

        // New instance preferred.
        int instanceId = MultiWindowUtils.getInstanceIdForViewIntent(true);
        assertEquals(
                "The default instance ID should be returned when a new instance is preferred.",
                MultiWindowUtils.INVALID_INSTANCE_ID,
                instanceId);

        // Existing instance preferred.
        instanceId = MultiWindowUtils.getInstanceIdForViewIntent(false);
        assertEquals(
                "The last accessed instance ID should be returned when an existing instance is"
                        + " preferred.",
                maxInstances - 2,
                instanceId);
    }

    @Test
    @Config(sdk = 31)
    public void testGetInstanceIdForViewIntent_MaxInstancesOpen_MaxRunningActivities() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        int maxInstances = MultiWindowUtils.getMaxInstances();
        // Simulate opening of max number of instances. #writeInstanceInfo will update the access
        // time for IDs 0 -> |maxInstances - 1| in increasing order of recency.
        for (int i = 0; i < maxInstances; i++) {
            ShadowMultiInstanceManagerApi31.updateWindowIdsOfRunningTabbedActivities(i, false);
            writeInstanceInfo(i, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, i);
        }

        // Simulate last access of instance ID 0.
        writeInstanceInfo(0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, 0);

        int instanceId = MultiWindowUtils.getInstanceIdForViewIntent(true);
        assertEquals("The last accessed instance ID should be returned.", 0, instanceId);
    }

    @Test
    @Config(sdk = 31)
    public void testGetInstanceIdForViewIntent_MaxInstancesOpen_LessThanMaxRunningActivities() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        int maxInstances = MultiWindowUtils.getMaxInstances();
        // Simulate opening of max number of instances. #writeInstanceInfo will update the access
        // time for IDs 0 -> |maxInstances - 1| in increasing order of recency.
        for (int i = 0; i < maxInstances; i++) {
            ShadowMultiInstanceManagerApi31.updateWindowIdsOfRunningTabbedActivities(i, false);
            writeInstanceInfo(i, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, i);
        }

        // Simulate last access of instance ID 0.
        writeInstanceInfo(0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, 0);
        // Simulate destruction of the activity represented by instance ID 0.
        ShadowMultiInstanceManagerApi31.updateWindowIdsOfRunningTabbedActivities(0, true);

        int instanceId = MultiWindowUtils.getInstanceIdForViewIntent(true);
        assertEquals(
                "The instance ID of a running activity that was last accessed should be returned.",
                maxInstances - 1,
                instanceId);
    }

    @Test
    @Config(sdk = 31)
    public void testGetInstanceIdForLinkIntent_LessThanMaxInstancesOpen() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        int maxInstances = MultiWindowUtils.getMaxInstances();
        // Simulate opening of 1 less than the max number of instances. #writeInstanceInfo will
        // update the access time for IDs 0 -> |maxInstances - 2| in increasing order of recency.
        for (int i = 0; i < maxInstances - 1; i++) {
            ShadowMultiInstanceManagerApi31.updateWindowIdsOfRunningTabbedActivities(i, false);
            writeInstanceInfo(i, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, i);
        }

        int instanceId = MultiWindowUtils.getInstanceIdForLinkIntent(mock(Activity.class));
        assertEquals(
                "Instance ID for link intent should be INVALID_INSTANCE_ID when fewer than the max"
                        + " number of instances are open.",
                MultiWindowUtils.INVALID_INSTANCE_ID,
                instanceId);
    }

    @Test
    @Config(sdk = 31)
    public void testRecordDesktopWindowCount_OnlyOnColdStart() {
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);

        // Simulate persistence of 2 instances, running of 1.
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 0, /* incognitoTabCount= */ 0, TASK_ID_6);
        int runningActivityCount = 1;
        ShadowMultiInstanceManagerApi31.updateRunningTabbedActivityCount(runningActivityCount);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW, runningActivityCount)
                        .expectIntRecord(HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW, 2)
                        .build();

        // Assume that the histograms are attempted to be recorded on a cold start of the app.
        MultiWindowUtils.maybeRecordDesktopWindowCountHistograms(
                mDesktopWindowStateProvider,
                InstanceAllocationType.NEW_INSTANCE_NEW_TASK,
                /* isColdStart= */ true);

        // Assume that the histograms are attempted to be recorded on a subsequent warm start.
        MultiWindowUtils.maybeRecordDesktopWindowCountHistograms(
                mDesktopWindowStateProvider,
                InstanceAllocationType.NEW_INSTANCE_NEW_TASK,
                /* isColdStart= */ false);

        // Each histogram should be emitted only once.
        watcher.assertExpected();
    }

    @Test
    @Config(sdk = 31)
    public void testRecordDesktopWindowCount_ColdStartOfExistingInstance() {
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);

        // Simulate persistence of 2 instances, running of 1.
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 0, /* incognitoTabCount= */ 0, TASK_ID_6);
        int runningActivityCount = 1;
        ShadowMultiInstanceManagerApi31.updateRunningTabbedActivityCount(runningActivityCount);

        int[] instanceAllocationTypes =
                new int[] {
                    InstanceAllocationType.DEFAULT,
                    InstanceAllocationType.EXISTING_INSTANCE_MAPPED_TASK,
                    InstanceAllocationType.EXISTING_INSTANCE_UNMAPPED_TASK,
                    InstanceAllocationType.EXISTING_INSTANCE_NEW_TASK
                };

        // Assume that the histograms are attempted to be recorded on a cold start of an existing
        // instance, for different instance allocation types.
        for (int type : instanceAllocationTypes) {
            var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW, runningActivityCount)
                            .expectIntRecord(
                                    HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW
                                            + HISTOGRAM_DESKTOP_WINDOW_COUNT_EXISTING_INSTANCE_SUFFIX,
                                    runningActivityCount)
                            .expectNoRecords(
                                    HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW
                                            + HISTOGRAM_DESKTOP_WINDOW_COUNT_NEW_INSTANCE_SUFFIX)
                            .expectIntRecord(HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW, 2)
                            .expectIntRecord(
                                    HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW
                                            + HISTOGRAM_DESKTOP_WINDOW_COUNT_EXISTING_INSTANCE_SUFFIX,
                                    2)
                            .expectNoRecords(
                                    HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW
                                            + HISTOGRAM_DESKTOP_WINDOW_COUNT_NEW_INSTANCE_SUFFIX)
                            .build();
            MultiWindowUtils.maybeRecordDesktopWindowCountHistograms(
                    mDesktopWindowStateProvider, type, /* isColdStart= */ true);
            watcher.assertExpected();
        }
    }

    @Test
    @Config(sdk = 31)
    public void testRecordDesktopWindowCount_ColdStartOfNewInstance() {
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);

        // Simulate persistence of 2 instances, running of 1.
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 0, /* incognitoTabCount= */ 0, TASK_ID_6);
        int runningActivityCount = 1;
        ShadowMultiInstanceManagerApi31.updateRunningTabbedActivityCount(runningActivityCount);

        int[] instanceAllocationTypes =
                new int[] {
                    InstanceAllocationType.NEW_INSTANCE_NEW_TASK,
                    InstanceAllocationType.PREFER_NEW_INSTANCE_NEW_TASK
                };

        // Assume that the histograms are attempted to be recorded on a cold start of a new
        // instance, for different instance allocation types.
        for (int type : instanceAllocationTypes) {
            var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW, runningActivityCount)
                            .expectIntRecord(
                                    HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW
                                            + HISTOGRAM_DESKTOP_WINDOW_COUNT_NEW_INSTANCE_SUFFIX,
                                    runningActivityCount)
                            .expectNoRecords(
                                    HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW
                                            + HISTOGRAM_DESKTOP_WINDOW_COUNT_EXISTING_INSTANCE_SUFFIX)
                            .expectIntRecord(HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW, 2)
                            .expectIntRecord(
                                    HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW
                                            + HISTOGRAM_DESKTOP_WINDOW_COUNT_NEW_INSTANCE_SUFFIX,
                                    2)
                            .expectNoRecords(
                                    HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW
                                            + HISTOGRAM_DESKTOP_WINDOW_COUNT_EXISTING_INSTANCE_SUFFIX)
                            .build();
            MultiWindowUtils.maybeRecordDesktopWindowCountHistograms(
                    mDesktopWindowStateProvider, type, /* isColdStart= */ true);
            watcher.assertExpected();
        }
    }

    @Test
    @Config(sdk = 31)
    public void testRecordDesktopWindowCount_NotInDesktopWindow() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW)
                        .expectNoRecords(
                                HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW
                                        + HISTOGRAM_DESKTOP_WINDOW_COUNT_EXISTING_INSTANCE_SUFFIX)
                        .expectNoRecords(
                                HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW
                                        + HISTOGRAM_DESKTOP_WINDOW_COUNT_NEW_INSTANCE_SUFFIX)
                        .expectNoRecords(HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW)
                        .expectNoRecords(
                                HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW
                                        + HISTOGRAM_DESKTOP_WINDOW_COUNT_EXISTING_INSTANCE_SUFFIX)
                        .expectNoRecords(
                                HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW
                                        + HISTOGRAM_DESKTOP_WINDOW_COUNT_NEW_INSTANCE_SUFFIX)
                        .build();

        // Assume that the histograms are attempted to be recorded on a cold start of the app, not
        // in a desktop window.
        MultiWindowUtils.maybeRecordDesktopWindowCountHistograms(
                mDesktopWindowStateProvider,
                InstanceAllocationType.NEW_INSTANCE_NEW_TASK,
                /* isColdStart= */ true);

        // Histograms should not be emitted.
        watcher.assertExpected();
    }

    private void writeInstanceInfo(
            int instanceId, String url, int tabCount, int incognitoTabCount, int taskId) {
        MultiInstanceManagerApi31.writeUrl(instanceId, url);
        when(mNormalTabModel.getCount()).thenReturn(tabCount);
        when(mIncognitoTabModel.getCount()).thenReturn(incognitoTabCount);
        MultiInstanceManagerApi31.writeLastAccessedTime(instanceId);
        MultiInstanceManagerApi31.writeTabCount(instanceId, mTabModelSelector);
        MultiInstanceManagerApi31.updateTaskMap(instanceId, taskId);
    }
}
