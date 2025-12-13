// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link MultiInstanceManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MultiInstanceManagerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mTabGroupSyncService;

    private Activity mActivity;

    private final ObservableSupplier<TabModelOrchestrator> mTabModelOrchestratorSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(any())).thenReturn(true);
    }

    @Test
    public void testCreateNewWindowIntent_incognito_throwsException() {
        MultiInstanceManagerImpl multiInstanceManager =
                new MultiInstanceManagerImpl(
                        mActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mMenuOrKeyboardActionController);

        assertThrows(
                AssertionError.class,
                () -> multiInstanceManager.createNewWindowIntent(/* isIncognito= */ true));
    }

    @Test
    public void testCreateNewWindowIntent_unsupportedWindowingMode_throwsException() {
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(false);
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(false);
        when(mMultiWindowModeStateDispatcher.isInMultiDisplayMode()).thenReturn(false);
        MultiInstanceManagerImpl multiInstanceManager =
                new MultiInstanceManagerImpl(
                        mActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mMenuOrKeyboardActionController);

        assertThrows(
                AssertionError.class,
                () -> multiInstanceManager.createNewWindowIntent(/* isIncognito= */ false));
    }

    @Test
    public void
            testCreateNewWindowIntent_nullIntentFromMultiWindowModeStateDispatcher_returnsNull() {
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.getOpenInOtherWindowIntent()).thenReturn(null);
        MultiInstanceManagerImpl multiInstanceManager =
                new MultiInstanceManagerImpl(
                        mActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mMenuOrKeyboardActionController);

        assertNull(multiInstanceManager.createNewWindowIntent(/* isIncognito= */ false));
    }

    @Test
    public void
            testCreateNewWindowIntent_nonMultiWindowMode_shouldNotOpenInAdjacentWindow_noLaunchAdjacentFlag() {
        when(mMultiWindowModeStateDispatcher.getOpenInOtherWindowIntent()).thenReturn(new Intent());

        // Non-multi-window mode.
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(false);
        Activity mockActivity = mock(Activity.class);
        when(mockActivity.isInMultiWindowMode()).thenReturn(false);

        // The new window shouldn't be opened as an adjacent window.
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);

        MultiInstanceManagerImpl multiInstanceManager =
                new MultiInstanceManagerImpl(
                        mockActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mMenuOrKeyboardActionController);

        Intent intent = multiInstanceManager.createNewWindowIntent(/* isIncognito= */ false);

        assertNotNull(intent);
        assertEquals(0, (intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT));
    }

    @Test
    public void
            testCreateNewWindowIntent_nonMultiWindowMode_shouldOpenInAdjacentWindow_addsLaunchAdjacentFlag() {
        when(mMultiWindowModeStateDispatcher.getOpenInOtherWindowIntent()).thenReturn(new Intent());

        // Non-multi-window mode.
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(false);
        Activity mockActivity = mock(Activity.class);
        when(mockActivity.isInMultiWindowMode()).thenReturn(false);

        // The new window should be opened as an adjacent window.
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                true);

        MultiInstanceManagerImpl multiInstanceManager =
                new MultiInstanceManagerImpl(
                        mockActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mMenuOrKeyboardActionController);

        Intent intent = multiInstanceManager.createNewWindowIntent(/* isIncognito= */ false);

        assertNotNull(intent);
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testCreateNewWindowIntent_multiWindowMode_addsLaunchAdjacentFlag() {
        when(mMultiWindowModeStateDispatcher.getOpenInOtherWindowIntent()).thenReturn(new Intent());

        // Multi-window mode.
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(true);
        Activity mockActivity = mock(Activity.class);
        when(mockActivity.isInMultiWindowMode()).thenReturn(true);

        MultiInstanceManagerImpl multiInstanceManager =
                new MultiInstanceManagerImpl(
                        mockActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mMenuOrKeyboardActionController);

        Intent intent = multiInstanceManager.createNewWindowIntent(/* isIncognito= */ false);

        assertNotNull(intent);
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testCleanupSyncedTabGroupsIfOnlyInstance() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(anyBoolean()))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        when(mMultiWindowModeStateDispatcher.isMultiInstanceRunning()).thenReturn(true);
        MultiInstanceManagerImpl multiInstanceManager =
                new MultiInstanceManagerImpl(
                        mActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mMenuOrKeyboardActionController);

        multiInstanceManager.cleanupSyncedTabGroupsIfOnlyInstance(mTabModelSelector);
        verifyNoInteractions(mTabGroupSyncService);

        when(mMultiWindowModeStateDispatcher.isMultiInstanceRunning()).thenReturn(false);
        multiInstanceManager.cleanupSyncedTabGroupsIfOnlyInstance(mTabModelSelector);
        verify(mTabGroupSyncService).getAllGroupIds();
    }
}
