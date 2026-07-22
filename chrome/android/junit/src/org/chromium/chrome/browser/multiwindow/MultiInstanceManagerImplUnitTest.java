// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtilsJni;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;

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
    @Mock private TabModelSelectorBase mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private IncognitoUtils.Natives mIncognitoUtilsJniMock;
    @Mock private MultiInstanceOrchestrator mMockMultiInstanceOrchestrator;
    @Mock private TabModelOrchestrator mTabModelOrchestrator;

    private Activity mActivity;

    private final SettableMonotonicObservableSupplier<TabModelOrchestrator> mTabModelOrchestratorSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(any())).thenReturn(true);
        IncognitoUtilsJni.setInstanceForTesting(mIncognitoUtilsJniMock);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMockMultiInstanceOrchestrator);
    }

    @Test
    public void testCleanupSyncedTabGroupsIfOnlyInstance() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabModelSelector.getModel(anyBoolean())).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        when(mMultiWindowModeStateDispatcher.isMultiInstanceRunning()).thenReturn(true);
        MultiInstanceManagerImpl multiInstanceManager = createMultiInstanceManager();

        multiInstanceManager.cleanupSyncedTabGroupsIfOnlyInstance(mTabModelSelector);
        verifyNoInteractions(mTabGroupSyncService);

        when(mMultiWindowModeStateDispatcher.isMultiInstanceRunning()).thenReturn(false);
        multiInstanceManager.cleanupSyncedTabGroupsIfOnlyInstance(mTabModelSelector);
        verify(mTabGroupSyncService).getAllGroupIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_MODE_FORCED_ANDROID)
    public void testCreateNewWindow_ForcedIncognito() {
        when(mIncognitoUtilsJniMock.getIncognitoModeForced(any())).thenReturn(true);
        when(mTabModelOrchestrator.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        mTabModelOrchestratorSupplier.set(mTabModelOrchestrator);

        MultiInstanceManagerImpl multiInstanceManager = createMultiInstanceManager();

        assertTrue(multiInstanceManager.handleMenuOrKeyboardAction(R.id.new_window_menu_id, true));

        verify(mMockMultiInstanceOrchestrator)
                .createNewWindow(
                        eq(mActivity),
                        /* isIncognito= */ eq(true),
                        /* additionalIntentExtras= */ isNull(),
                        /* startActivityOptions= */ isNull(),
                        /* source= */ eq(NewWindowAppSource.MENU));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_MODE_FORCED_ANDROID)
    public void testCreateNewWindow_OrchestratorNull_Aborts() {
        MultiInstanceManagerImpl multiInstanceManager = createMultiInstanceManager();

        assertTrue(multiInstanceManager.handleMenuOrKeyboardAction(R.id.new_window_menu_id, true));

        verifyNoInteractions(mMockMultiInstanceOrchestrator);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_MODE_FORCED_ANDROID)
    public void testCreateNewWindow_ProfileNull_Aborts() {
        when(mTabModelOrchestrator.getTabModelSelector()).thenReturn(null);
        mTabModelOrchestratorSupplier.set(mTabModelOrchestrator);

        MultiInstanceManagerImpl multiInstanceManager = createMultiInstanceManager();

        assertTrue(multiInstanceManager.handleMenuOrKeyboardAction(R.id.new_window_menu_id, true));

        verifyNoInteractions(mMockMultiInstanceOrchestrator);
    }

    private MultiInstanceManagerImpl createMultiInstanceManager() {
        return new MultiInstanceManagerImpl(
                mActivity,
                mTabModelOrchestratorSupplier,
                mMultiWindowModeStateDispatcher,
                mActivityLifecycleDispatcher,
                mMenuOrKeyboardActionController);
    }
}
