// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.Window;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link ChromeTabbedActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeTabbedActivityUnitTest {
    private static class TestChromeTabbedActivity extends ChromeTabbedActivity {
        private boolean mAreTabModelsInitialized;
        private boolean mIsActivityFinishingOrDestroyed;
        private boolean mDidFinishNativeInitialization;
        private boolean mTerminateIncognitoSessionCalled;
        private boolean mFinishCalled;
        private final List<Integer> mRequestedWindowFeatures = new ArrayList<>();

        public List<Integer> getRequestedWindowFeatures() {
            return mRequestedWindowFeatures;
        }

        // AppCompatDelegate#hasWindowFeature() cannot be queried before onCreate(), so record the
        // request instead (intentionally without calling super).
        @Override
        public boolean supportRequestWindowFeature(int featureId) {
            mRequestedWindowFeatures.add(featureId);
            return true;
        }

        public void setAreTabModelsInitialized(boolean areTabModelsInitialized) {
            mAreTabModelsInitialized = areTabModelsInitialized;
        }

        public void setIsActivityFinishingOrDestroyed(boolean isActivityFinishingOrDestroyed) {
            mIsActivityFinishingOrDestroyed = isActivityFinishingOrDestroyed;
        }

        public void setDidFinishNativeInitialization(boolean didFinishNativeInitialization) {
            mDidFinishNativeInitialization = didFinishNativeInitialization;
        }

        public boolean wasTerminateIncognitoSessionCalled() {
            return mTerminateIncognitoSessionCalled;
        }

        public boolean wasFinishCalled() {
            return mFinishCalled;
        }

        @Override
        public boolean areTabModelsInitialized() {
            return mAreTabModelsInitialized;
        }

        @Override
        public boolean isActivityFinishingOrDestroyed() {
            return mIsActivityFinishingOrDestroyed;
        }

        @Override
        public boolean didFinishNativeInitialization() {
            return mDidFinishNativeInitialization;
        }

        @Override
        public void terminateIncognitoSession() {
            mTerminateIncognitoSessionCalled = true;
        }

        @Override
        public void finish() {
            mFinishCalled = true;
        }
    }

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
        IncognitoTabHostRegistry.getInstance().clearForTesting();
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
    public void testEducationTipModuleActionDelegate_openHubPane_disabledOnDesktop_suppressesHub() {
        DeviceInfo.setIsDesktopForTesting(true);
        EducationTipModuleActionDelegate delegate =
                mActivity.createEducationTipModuleActionDelegate();
        // Should return early without throwing any NPE or calling layout manager
        delegate.openHubPane(PaneId.TAB_GROUPS);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void
            testEducationTipModuleActionDelegate_openTabGroupIphDialog_disabledOnDesktop_suppressesIphAndHub() {
        DeviceInfo.setIsDesktopForTesting(true);
        EducationTipModuleActionDelegate delegate =
                mActivity.createEducationTipModuleActionDelegate();
        // Should return early without throwing any NPE or creating IPH coordinator
        delegate.openTabGroupIphDialog();
    }

    @Test
    public void testDestroyTabModels_unregistersIncognitoTabHost() {
        IncognitoTabHost host = mActivity.getIncognitoTabHostForTesting();
        IncognitoTabHostRegistry.getInstance().register(host);
        assertTrue(IncognitoTabHostRegistry.getInstance().getHosts().contains(host));

        mActivity.destroyTabModels();

        assertFalse(IncognitoTabHostRegistry.getInstance().getHosts().contains(host));
    }

    @Test
    public void testDestroyTabModels_unregistersEvenIfOrchestratorThrows() {
        TabModelOrchestrator orchestrator = mock(TabModelOrchestrator.class);
        doThrow(new RuntimeException("destroy failure")).when(orchestrator).destroy();
        TestChromeTabbedActivity activity = new TestChromeTabbedActivity();
        activity.setTabModelOrchestratorForTesting(orchestrator);
        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        IncognitoTabHostRegistry.getInstance().register(host);
        assertTrue(IncognitoTabHostRegistry.getInstance().getHosts().contains(host));

        assertThrows(RuntimeException.class, activity::destroyTabModels);
        verify(orchestrator).destroy();

        assertFalse(IncognitoTabHostRegistry.getInstance().getHosts().contains(host));
    }

    @Test
    public void testIncognitoTabHost_uninitializedTabModels() {
        IncognitoTabHost host = mActivity.getIncognitoTabHostForTesting();
        assertFalse(mActivity.areTabModelsInitialized());
        assertFalse(host.hasIncognitoTabs());
        assertFalse(host.isActiveModel());
    }

    @Test
    public void testIncognitoTabHost_initializedTabModels() {
        TestChromeTabbedActivity activity = new TestChromeTabbedActivity();
        activity.setAreTabModelsInitialized(true);
        TabModelSelectorBase tabModelSelector = mock(TabModelSelectorBase.class);
        TabModel incognitoTabModel = mock(TabModel.class);
        when(tabModelSelector.getModel(/* incognito= */ true)).thenReturn(incognitoTabModel);
        when(incognitoTabModel.getCount()).thenReturn(2);
        when(incognitoTabModel.isActiveModel()).thenReturn(true);
        activity.setTabModelSelectorForTesting(tabModelSelector);

        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        assertTrue(host.hasIncognitoTabs());
        assertTrue(host.isActiveModel());

        // An activity that is finishing must still report living incognito tabs until tab models
        // are actually destroyed.
        activity.setIsActivityFinishingOrDestroyed(true);
        assertTrue(host.hasIncognitoTabs());

        when(incognitoTabModel.getCount()).thenReturn(0);
        when(incognitoTabModel.isActiveModel()).thenReturn(false);
        assertFalse(host.hasIncognitoTabs());
        assertFalse(host.isActiveModel());
    }

    @Test
    public void
            testCloseAllIncognitoTabs_initialized_terminatesSessionEvenIfFinishingOrDestroyed() {
        TestChromeTabbedActivity activity = new TestChromeTabbedActivity();
        activity.setDidFinishNativeInitialization(true);
        activity.setAreTabModelsInitialized(true);
        activity.setIsActivityFinishingOrDestroyed(true);
        TabModelSelectorBase tabModelSelector = mock(TabModelSelectorBase.class);
        activity.setTabModelSelectorForTesting(tabModelSelector);

        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        host.closeAllIncognitoTabs();

        assertTrue(activity.wasTerminateIncognitoSessionCalled());
        assertFalse(activity.wasFinishCalled());
    }

    @Test
    public void
            testCloseAllIncognitoTabs_nativeNotInitialized_delegatesToCloseAllIncognitoTabsOnInit() {
        TestChromeTabbedActivity activity = new TestChromeTabbedActivity();
        activity.setDidFinishNativeInitialization(false);
        activity.setAreTabModelsInitialized(true);
        activity.setIsActivityFinishingOrDestroyed(true);
        TabModelSelectorBase tabModelSelector = mock(TabModelSelectorBase.class);
        activity.setTabModelSelectorForTesting(tabModelSelector);

        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        host.closeAllIncognitoTabs();

        assertFalse(activity.wasTerminateIncognitoSessionCalled());
        assertFalse(activity.wasFinishCalled());
    }

    @Test
    public void testCloseAllIncognitoTabs_uninitialized_finishesIfNotFinishing() {
        TestChromeTabbedActivity activity = new TestChromeTabbedActivity();
        activity.setDidFinishNativeInitialization(true);
        activity.setAreTabModelsInitialized(false);
        activity.setIsActivityFinishingOrDestroyed(false);

        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        host.closeAllIncognitoTabs();

        assertFalse(activity.wasTerminateIncognitoSessionCalled());
        assertTrue(activity.wasFinishCalled());
    }

    @Test
    public void testCloseAllIncognitoTabs_uninitialized_noOpIfFinishingOrDestroyed() {
        TestChromeTabbedActivity activity = new TestChromeTabbedActivity();
        activity.setDidFinishNativeInitialization(true);
        activity.setAreTabModelsInitialized(false);
        activity.setIsActivityFinishingOrDestroyed(true);

        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        host.closeAllIncognitoTabs();

        assertFalse(activity.wasTerminateIncognitoSessionCalled());
        assertFalse(activity.wasFinishCalled());
    }

    @Test
    public void testCloseAllIncognitoTabsOnInit_finishingOrDestroyed_noOp() {
        TestChromeTabbedActivity activity = new TestChromeTabbedActivity();
        activity.setIsActivityFinishingOrDestroyed(true);

        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        host.closeAllIncognitoTabsOnInit();

        assertFalse(activity.wasTerminateIncognitoSessionCalled());
        assertFalse(activity.wasFinishCalled());
    }

    @Test
    public void
            testCloseAllIncognitoTabsOnInit_initializedAndTabStateInitialized_terminatesSession() {
        TestChromeTabbedActivity activity = new TestChromeTabbedActivity();
        activity.setDidFinishNativeInitialization(true);
        TabModelSelectorBase tabModelSelector = mock(TabModelSelectorBase.class);
        when(tabModelSelector.isTabStateInitialized()).thenReturn(true);
        activity.setTabModelSelectorForTesting(tabModelSelector);

        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        host.closeAllIncognitoTabsOnInit();

        assertTrue(activity.wasTerminateIncognitoSessionCalled());
        assertFalse(activity.wasFinishCalled());
    }

    @Test
    public void testOnPreCreate_requestsFeatureActionModeOverlay() {
        TestChromeTabbedActivity activity =
                Robolectric.buildActivity(TestChromeTabbedActivity.class).get();
        activity.onPreCreate();
        assertTrue(
                activity.getRequestedWindowFeatures().contains(Window.FEATURE_ACTION_MODE_OVERLAY));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void testShouldRemoveWindowWithZeroTabs_gtsDisabledOnDesktop_quickDeleteClosesWindow() {
        // Normal profile on desktop case.
        DeviceInfo.setIsDesktopForTesting(true);
        mActivity.setSupportedProfileTypeForTesting(SupportedProfileType.REGULAR);
        assertFalse(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.GRID_TAB_SWITCHER));
        assertFalse(
                mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.BOTTOM_TAB_GROUP_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.QUICK_DELETE));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.CLEAR_BROWSING_DATA));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.TABLET_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.KEYBOARD_SHORTCUT));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.VERTICAL_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.OPEN_IN_APP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.UNKNOWN));

        // Off-the-record profile case (Verify it closes unconditionally).
        mActivity.setSupportedProfileTypeForTesting(SupportedProfileType.OFF_THE_RECORD);
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.GRID_TAB_SWITCHER));
        assertTrue(
                mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.BOTTOM_TAB_GROUP_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.QUICK_DELETE));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.CLEAR_BROWSING_DATA));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.TABLET_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.KEYBOARD_SHORTCUT));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.VERTICAL_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.OPEN_IN_APP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.UNKNOWN));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void
            testShouldRemoveWindowWithZeroTabs_gtsEnabledOnDesktop_quickDeleteDoesNotCloseWindow() {
        // Normal profile on desktop case.
        DeviceInfo.setIsDesktopForTesting(true);
        mActivity.setSupportedProfileTypeForTesting(SupportedProfileType.REGULAR);
        assertFalse(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.GRID_TAB_SWITCHER));
        assertFalse(
                mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.BOTTOM_TAB_GROUP_STRIP));
        assertFalse(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.QUICK_DELETE));
        assertFalse(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.CLEAR_BROWSING_DATA));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.TABLET_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.KEYBOARD_SHORTCUT));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.VERTICAL_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.OPEN_IN_APP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.UNKNOWN));

        // Off-the-record profile case (Verify it closes unconditionally).
        mActivity.setSupportedProfileTypeForTesting(SupportedProfileType.OFF_THE_RECORD);
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.GRID_TAB_SWITCHER));
        assertTrue(
                mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.BOTTOM_TAB_GROUP_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.QUICK_DELETE));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.CLEAR_BROWSING_DATA));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.TABLET_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.KEYBOARD_SHORTCUT));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.VERTICAL_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.OPEN_IN_APP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.UNKNOWN));
    }

    @Test
    public void testShouldRemoveWindowWithZeroTabs_nonDesktop_quickDeleteDoesNotCloseWindow() {
        // Normal profile on non-desktop case (GTS always enabled on non-desktop).
        DeviceInfo.setIsDesktopForTesting(false);
        mActivity.setSupportedProfileTypeForTesting(SupportedProfileType.REGULAR);
        assertFalse(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.GRID_TAB_SWITCHER));
        assertFalse(
                mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.BOTTOM_TAB_GROUP_STRIP));
        assertFalse(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.QUICK_DELETE));
        assertFalse(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.CLEAR_BROWSING_DATA));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.TABLET_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.KEYBOARD_SHORTCUT));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.VERTICAL_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.OPEN_IN_APP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.UNKNOWN));

        // Off-the-record profile case (Verify it closes unconditionally even when GTS is enabled)
        mActivity.setSupportedProfileTypeForTesting(SupportedProfileType.OFF_THE_RECORD);
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.GRID_TAB_SWITCHER));
        assertTrue(
                mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.BOTTOM_TAB_GROUP_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.QUICK_DELETE));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.CLEAR_BROWSING_DATA));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.TABLET_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.KEYBOARD_SHORTCUT));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.VERTICAL_TAB_STRIP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.OPEN_IN_APP));
        assertTrue(mActivity.shouldRemoveWindowWithZeroTabs(TabClosingSource.UNKNOWN));
    }
}
