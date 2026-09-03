// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtilsJni;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link TabOverflowMenuCoordinator} UI changes. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.INCOGNITO_MODE_FORCED_ANDROID)
public class TabOverflowMenuCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private IncognitoUtils.Natives mIncognitoUtilsJniMock;
    @Mock private Supplier<TabModel> mTabModelSupplier;
    @Mock private MultiInstanceOrchestrator mMockMultiInstanceOrchestrator;
    @Mock private CollaborationService mCollaborationService;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;

    private Activity mActivity;

    private static class TestTabOverflowMenuCoordinator extends TabOverflowMenuCoordinator<Integer> {
        public TestTabOverflowMenuCoordinator(
                Activity activity,
                Supplier<TabModel> tabModelSupplier,
                MultiInstanceManager multiInstanceManager,
                TabGroupSyncService tabGroupSyncService,
                CollaborationService collaborationService) {
            super(
                    0,
                    0,
                    null,
                    tabModelSupplier,
                    multiInstanceManager,
                    tabGroupSyncService,
                    collaborationService,
                    activity);
        }

        @Override
        public void buildMenuActionItems(ModelList itemList, Integer id) {}

        @Override
        public void onMenuDismissed() {}

        @Override
        @Nullable
        public String getCollaborationIdOrNull(Integer id) {
            return null;
        }

        @Override
        protected int getMenuWidth(int anchorViewWidthPx) {
            return 0;
        }

        public ListItem testCreateMoveToWindowItem(
                Integer id, boolean isIncognito, boolean allowMoveToNewWindow) {
            return createMoveToWindowItem(
                    id, isIncognito, R.plurals.move_tab_to_another_window, 0, allowMoveToNewWindow);
        }
    }

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        when(mTabModelSupplier.get()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        IncognitoUtilsJni.setInstanceForTesting(mIncognitoUtilsJniMock);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMockMultiInstanceOrchestrator);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_MODE_FORCED_ANDROID)
    public void testCreateMoveToWindowItem_ForcedIncognito() {
        doReturn(true).when(mIncognitoUtilsJniMock).getIncognitoModeForced(any());

        // Mock at least 2 instances to trigger the submenu.
        List<InstanceInfo> instances = new ArrayList<>();
        instances.add(
                new InstanceInfo(
                        0, 0, InstanceInfo.Type.CURRENT, "win1", "win1", null, 1, 0, false, 0, 0));
        instances.add(
                new InstanceInfo(
                        1, 1, InstanceInfo.Type.OTHER, "win2", "win2", null, 1, 0, false, 0, 0));
        when(mMultiInstanceManager.getInstanceInfo(anyInt())).thenReturn(instances);

        TestTabOverflowMenuCoordinator coordinator =
                new TestTabOverflowMenuCoordinator(
                        mActivity,
                        mTabModelSupplier,
                        mMultiInstanceManager,
                        mTabGroupSyncService,
                        mCollaborationService);
        ListItem item = coordinator.testCreateMoveToWindowItem(1, false, true);

        List<ListItem> submenu = item.model.get(ListMenuSubmenuItemProperties.SUBMENU_PROVIDER).get();
        ListItem newWindowItem = submenu.get(0);

        assertEquals(
                R.string.menu_new_incognito_window,
                newWindowItem.model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.drawable.ic_domain,
                newWindowItem.model.get(ListMenuItemProperties.START_ICON_ID));
    }

    @Test
    public void testMoveAndCleanupSource_withMultiInstanceManager() {
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(1);
        boolean[] actionExecuted = new boolean[1];

        TabOverflowMenuCoordinator.moveAndCleanupSource(
                mMultiInstanceManager, () -> actionExecuted[0] = true);

        assertTrue(actionExecuted[0]);
        verify(mMultiInstanceManager).closeChromeWindowIfEmpty(1);
    }
}
