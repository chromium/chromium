// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtilsUnitTest.ShadowHomepageManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtilsUnitTest.ShadowMultiInstanceManagerApi31;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

/** Unit tests for {@link MultiWindowUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowMultiInstanceManagerApi31.class, ShadowHomepageManager.class})
public class MultiWindowUtilsUnitTest {
    /** Shadows {@link MultiInstanceManagerApi31} class for testing. */
    @Implements(MultiInstanceManagerApi31.class)
    public static class ShadowMultiInstanceManagerApi31 {
        private static SparseIntArray sWindowIdsOfRunningTabbedActivities;

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

        public static void reset() {
            sWindowIdsOfRunningTabbedActivities = null;
        }

        @Implementation
        public static SparseIntArray getWindowIdsOfRunningTabbedActivities() {
            return sWindowIdsOfRunningTabbedActivities;
        }
    }

    /** Shadow for {@link HomepageManager}. */
    @Implements(HomepageManager.class)
    public static class ShadowHomepageManager {
        static GURL sHomepageGurl;
        static boolean sIsHomepageEnabled;

        @Implementation
        public static boolean isHomepageEnabled() {
            return sIsHomepageEnabled;
        }

        @Implementation
        public static GURL getHomepageGurl() {
            return sHomepageGurl;
        }

        public static void reset() {
            sIsHomepageEnabled = false;
            sHomepageGurl = NTP_GURL;
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
        ShadowHomepageManager.sIsHomepageEnabled = true;
        ShadowHomepageManager.sHomepageGurl = NTP_GURL;
    }

    @After
    public void tearDown() {
        ShadowMultiInstanceManagerApi31.reset();
        ShadowHomepageManager.reset();
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
        ShadowHomepageManager.sIsHomepageEnabled = false;
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
        ShadowHomepageManager.sHomepageGurl = TEST_GURL;
        ShadowHomepageManager.sIsHomepageEnabled = true;
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
    public void testHasAtMostOneTabWithHomepageEnabledAsCustomUrl_ReturnsTrue() {
        ShadowHomepageManager.sHomepageGurl = TEST_GURL;
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        assertTrue(
                "Should return true for last tab with homepage set to a custom url.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabledAsNtp_ReturnsFalse() {
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        assertFalse(
                "Should return true for last tab with homepage as NTP.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void
            testHasAtMostOneTabWithHomepageEnabledAsCustomUrl_WithMoreThanOneTab_ReturnsFalse() {
        ShadowHomepageManager.sHomepageGurl = TEST_GURL;
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        assertFalse(
                "Should return false for multiple tabs.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void testHasAtMostOneTabWith_WithHomepageDisabled_ReturnsFalse() {
        ShadowHomepageManager.sIsHomepageEnabled = false;
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        assertFalse(
                "Should return false for homepage disabled.",
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
