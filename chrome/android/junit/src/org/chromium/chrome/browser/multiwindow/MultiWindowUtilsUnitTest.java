// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Unit tests for {@link MultiWindowUtilsUnit}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MultiWindowUtilsUnitTest {
    private static final int INSTANCE_ID_0 = 0;
    private static final int INSTANCE_ID_1 = 1;
    private static final int INSTANCE_ID_2 = 2;
    private static final int TASK_ID_5 = 5;
    private static final int TASK_ID_6 = 6;
    private static final int TASK_ID_7 = 7;
    private static final String URL_1 = "url1";
    private static final String URL_2 = "url2";
    private static final String URL_3 = "url3";

    private MultiWindowUtils mUtils;
    private boolean mIsInMultiWindowMode;
    private boolean mIsInMultiDisplayMode;
    private boolean mIsMultipleInstanceRunning;
    private boolean mIsAutosplitSupported;
    private boolean mCustomMultiWindowSupported;

    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    TabModel mNormalTabModel;
    @Mock
    TabModel mIncognitoTabModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mUtils = new MultiWindowUtils() {
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
            public Class<? extends Activity> getOpenInOtherWindowActivity(Activity current) {
                return Activity.class;
            }
        };
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
                    canEnter, mUtils.canEnterMultiWindowMode(null));
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
            assertEquals("multi-window: " + mIsInMultiWindowMode
                            + " multi-display: " + mIsInMultiDisplayMode
                            + " multi-instance: " + mIsMultipleInstanceRunning,
                    openInOtherWindow, mUtils.isOpenInOtherWindowSupported(null));
        }
    }

    @Test
    public void testGetInstanceCount() {
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        // Instance with no tabs (ID_1) still counts as long as it is alive.
        writeInstanceInfo(INSTANCE_ID_0, URL_1, /*tabCount=*/3, /*incognitoTabCount=*/2, TASK_ID_5);
        writeInstanceInfo(INSTANCE_ID_1, URL_2, /*tabCount=*/0, /*incognitoTabCount=*/0, TASK_ID_6);
        writeInstanceInfo(INSTANCE_ID_2, URL_3, /*tabCount=*/6, /*incognitoTabCount=*/2, TASK_ID_7);
        assertEquals(3, MultiWindowUtils.getInstanceCount());

        // Instance with no running task is not taken into account if there is no normal tab,
        // regardless of the # of incognito tabs.
        writeInstanceInfo(INSTANCE_ID_1, URL_2, /*tabCount=*/0, /*incognitoTabCount=*/2,
                MultiWindowUtils.INVALID_TASK_ID);
        assertEquals(2, MultiWindowUtils.getInstanceCount());
    }

    @Test
    public void testGetInstanceIdForViewIntent_LesserThanMaxWindowsOpen() {
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        int maxInstances = MultiWindowUtils.getMaxInstances();
        // Simulate opening of 1 less than the max number of instances.
        for (int i = 0; i < maxInstances - 1; i++) {
            writeInstanceInfo(i, URL_1, /*tabCount=*/3, /*incognitoTabCount=*/0, i + 5);
        }

        int instanceId = MultiWindowUtils.getInstanceIdForViewIntent();
        assertEquals("The default instance ID should be returned.",
                MultiWindowUtils.INVALID_INSTANCE_ID, instanceId);
    }

    @Test
    public void testGetInstanceIdForViewIntent_MaxWindowsOpen() {
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        int maxInstances = MultiWindowUtils.getMaxInstances();
        // Simulate opening of max number of instances.
        for (int i = 0; i < maxInstances; i++) {
            writeInstanceInfo(i, URL_1, /*tabCount=*/3, /*incognitoTabCount=*/0, i + 5);
        }

        // Simulate last access of instance ID 0.
        writeInstanceInfo(INSTANCE_ID_0, URL_1, /*tabCount=*/3, /*incognitoTabCount=*/0, TASK_ID_5);

        int instanceId = MultiWindowUtils.getInstanceIdForViewIntent();
        assertEquals(
                "The last accessed instance ID should be returned.", INSTANCE_ID_0, instanceId);
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
