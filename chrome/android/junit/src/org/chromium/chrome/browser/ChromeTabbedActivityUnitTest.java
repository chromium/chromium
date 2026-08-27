// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabsActionDelegate;

/** Unit tests for {@link ChromeTabbedActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeTabbedActivityUnitTest {
    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() {
        mActivity = new ChromeTabbedActivity();
        ProfileManager.resetForTesting();
    }

    @After
    public void tearDown() {
        ProfileManager.resetForTesting();
        DeviceInfo.resetIsDesktopForTesting();
    }

    @Test
    public void testTransformSavedInstanceStateForOnCreate_nullState() {
        assertNull(mActivity.transformSavedInstanceStateForOnCreate(null));
    }

    @Test
    public void testTransformSavedInstanceStateForOnCreate_profileManagerNotInitialized() {
        assertFalse(ProfileManager.isInitialized());

        Bundle savedState = new Bundle();
        savedState.putBundle("android:support:fragments", new Bundle());
        savedState.putString("custom_key", "custom_value");

        assertNull(mActivity.transformSavedInstanceStateForOnCreate(savedState));
    }

    @Test
    public void testTransformSavedInstanceStateForOnCreate_profileManagerInitialized() {
        Profile profile = mock(Profile.class);
        ProfileManager.setLastUsedProfileForTesting(profile);
        assertTrue(ProfileManager.isInitialized());

        Bundle savedState = new Bundle();
        savedState.putBundle("android:support:fragments", new Bundle());
        savedState.putString("custom_key", "custom_value");

        Bundle result = mActivity.transformSavedInstanceStateForOnCreate(savedState);

        assertNotNull(result);
        assertTrue(result.containsKey("android:support:fragments"));
        assertEquals("custom_value", result.getString("custom_key"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void testVerticalTabsActionDelegate_openHubSearch_disabledOnDesktop_doesNotTriggerHubSearch() {
        DeviceInfo.setIsDesktopForTesting(true);
        ChromeTabbedActivity activitySpy = spy(mActivity);
        doReturn(true).when(activitySpy).onMenuOrKeyboardAction(anyInt(), anyBoolean());

        VerticalTabsActionDelegate delegate = activitySpy.createVerticalTabsActionDelegate();
        delegate.openHubSearch();

        verify(activitySpy, never()).onMenuOrKeyboardAction(anyInt(), anyBoolean());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void testVerticalTabsActionDelegate_openHubSearch_enabledOnNonDesktop_triggersHubSearch() {
        DeviceInfo.setIsDesktopForTesting(false);
        ChromeTabbedActivity activitySpy = spy(mActivity);
        doReturn(true).when(activitySpy).onMenuOrKeyboardAction(anyInt(), anyBoolean());

        VerticalTabsActionDelegate delegate = activitySpy.createVerticalTabsActionDelegate();
        delegate.openHubSearch();

        verify(activitySpy).onMenuOrKeyboardAction(eq(R.id.tab_search), eq(false));
    }
}
