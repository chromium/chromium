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
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.Window;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabsActionDelegate;

/** Unit tests for {@link ChromeTabbedActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeTabbedActivityUnitTest {
    private static class TestChromeTabbedActivity extends ChromeTabbedActivity {
        private boolean mAreTabModelsInitialized;
        private boolean mIsActivityFinishingOrDestroyed;
        private boolean mDidFinishNativeInitialization;
        private boolean mTerminateIncognitoSessionCalled;
        private boolean mFinishCalled;

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

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private TabModelOrchestrator mTabModelOrchestrator;
    @Mock private TabModelSelectorBase mTabModelSelectorBase;
    @Mock private TabModel mTabModel;
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
        ProfileManager.setLastUsedProfileForTesting(mProfile);
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
    public void
            testVerticalTabsActionDelegate_openHubSearch_disabledOnDesktop_doesNotTriggerHubSearch() {
        DeviceInfo.setIsDesktopForTesting(true);
        ChromeTabbedActivity activitySpy = spy(mActivity);
        doReturn(true).when(activitySpy).onMenuOrKeyboardAction(anyInt(), anyBoolean());

        VerticalTabsActionDelegate delegate = activitySpy.createVerticalTabsActionDelegate();
        delegate.openHubSearch();

        verify(activitySpy, never()).onMenuOrKeyboardAction(anyInt(), anyBoolean());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void
            testVerticalTabsActionDelegate_openHubSearch_enabledOnNonDesktop_triggersHubSearch() {
        DeviceInfo.setIsDesktopForTesting(false);
        ChromeTabbedActivity activitySpy = spy(mActivity);
        doReturn(true).when(activitySpy).onMenuOrKeyboardAction(anyInt(), anyBoolean());

        VerticalTabsActionDelegate delegate = activitySpy.createVerticalTabsActionDelegate();
        delegate.openHubSearch();

        verify(activitySpy).onMenuOrKeyboardAction(eq(R.id.tab_search), /* fromMenu= */ eq(false));
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
        doThrow(new RuntimeException("destroy failure")).when(mTabModelOrchestrator).destroy();
        TestChromeTabbedActivity activity = new TestChromeTabbedActivity();
        activity.setTabModelOrchestratorForTesting(mTabModelOrchestrator);
        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        IncognitoTabHostRegistry.getInstance().register(host);
        assertTrue(IncognitoTabHostRegistry.getInstance().getHosts().contains(host));

        assertThrows(RuntimeException.class, activity::destroyTabModels);
        verify(mTabModelOrchestrator).destroy();

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
        when(mTabModelSelectorBase.getModel(/* incognito= */ true)).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.isActiveModel()).thenReturn(true);
        activity.setTabModelSelectorForTesting(mTabModelSelectorBase);

        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        assertTrue(host.hasIncognitoTabs());
        assertTrue(host.isActiveModel());

        // An activity that is finishing must still report living incognito tabs until tab models
        // are actually destroyed.
        activity.setIsActivityFinishingOrDestroyed(true);
        assertTrue(host.hasIncognitoTabs());

        when(mTabModel.getCount()).thenReturn(0);
        when(mTabModel.isActiveModel()).thenReturn(false);
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
        activity.setTabModelSelectorForTesting(mTabModelSelectorBase);

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
        activity.setTabModelSelectorForTesting(mTabModelSelectorBase);

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
        when(mTabModelSelectorBase.isTabStateInitialized()).thenReturn(true);
        activity.setTabModelSelectorForTesting(mTabModelSelectorBase);

        IncognitoTabHost host = activity.getIncognitoTabHostForTesting();
        host.closeAllIncognitoTabsOnInit();

        assertTrue(activity.wasTerminateIncognitoSessionCalled());
        assertFalse(activity.wasFinishCalled());
    }

    @Test
    public void testOnPreCreate_requestsFeatureActionModeOverlay() {
        ChromeTabbedActivity activity =
                spy(Robolectric.buildActivity(ChromeTabbedActivity.class).get());
        activity.onPreCreate();
        verify(activity).supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);
    }
}
