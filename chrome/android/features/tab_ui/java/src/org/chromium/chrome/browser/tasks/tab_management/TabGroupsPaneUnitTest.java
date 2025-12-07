// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;
import java.util.function.Supplier;

/** Unit tests for {@link TabGroupsPane}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
@DisableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
public class TabGroupsPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabCreator mTabCreator;
    @Mock private DoubleConsumer mOnToolbarAlphaChange;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Supplier<PaneManager> mPaneManagerSupplier;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock Supplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    @Mock FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock SyncService mSyncService;
    @Mock ModalDialogManager mModalDialogManager;
    @Mock TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock EdgeToEdgeController mEdgeToEdgeController;
    @Mock Tab mTab;

    private final OneshotSupplierImpl<ProfileProvider> mProfileSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<ModalDialogManager> mModalDialogManagerSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    private TabGroupsPane mTabGroupsPane;

    @Before
    public void setUp() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        ApplicationProvider.getApplicationContext().setTheme(R.style.Theme_BrowserUI_DayNight);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        mProfileSupplier.set(mProfileProvider);
        mModalDialogManagerSupplier.set(mModalDialogManager);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);

        // Unused at this level.
        when(mTabGroupSyncService.getVersioningMessageController()).thenReturn(mock());

        mTabGroupsPane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange,
                        mProfileSupplier,
                        mPaneManagerSupplier,
                        mTabGroupUiActionHandlerSupplier,
                        mModalDialogManagerSupplier,
                        mEdgeToEdgeSupplier,
                        mDataSharingTabManager);
    }

    @Test
    public void testNotifyLoadHint() {
        assertEquals(0, mTabGroupsPane.getRootView().getChildCount());

        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        assertNotEquals(0, mTabGroupsPane.getRootView().getChildCount());

        mTabGroupsPane.notifyLoadHint(LoadHint.COLD);
        assertEquals(0, mTabGroupsPane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileHot() {
        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        mTabGroupsPane.destroy();
        assertEquals(0, mTabGroupsPane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileCold() {
        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        mTabGroupsPane.notifyLoadHint(LoadHint.COLD);
        mTabGroupsPane.destroy();
        assertEquals(0, mTabGroupsPane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_NoLoadHint() {
        mTabGroupsPane.destroy();
        assertEquals(0, mTabGroupsPane.getRootView().getChildCount());
    }

    @Test
    public void testEdgeToEdgePadAdjuster_BeforeLoadHint() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        assertFalse(mEdgeToEdgeSupplier.hasObservers());

        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        assertTrue(mEdgeToEdgeSupplier.hasObservers());
        ShadowLooper.idleMainLooper();
        verify(mEdgeToEdgeController).registerAdjuster(notNull());
    }

    @Test
    public void testEdgeToEdgePadAdjuster_AfterLoadHint() {
        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        assertTrue(mEdgeToEdgeSupplier.hasObservers());

        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(notNull());
    }

    @Test
    public void testEdgeToEdgePadAdjuster_ChangeController() {
        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(notNull());

        EdgeToEdgeController controller2 = mock(EdgeToEdgeController.class);
        mEdgeToEdgeSupplier.set(controller2);
        verify(controller2).registerAdjuster(notNull());
        verify(mEdgeToEdgeController).unregisterAdjuster(notNull());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testNewTabGroupButtonDisabled() {
        assertNull(mTabGroupsPane.getActionButtonDataSupplier().get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testNewTabGroupButtonEnabled() {
        assertNotNull(mTabGroupsPane.getActionButtonDataSupplier().get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testNewTabGroupButton() {
        when(mTabCreator.createNewTab(any(), anyInt(), any())).thenReturn(mTab);
        FullButtonData actionButtonData = mTabGroupsPane.getActionButtonDataSupplier().get();
        assertNotNull(actionButtonData);

        Runnable onPressRunnable = actionButtonData.getOnPressRunnable();
        assertNotNull(onPressRunnable);
        onPressRunnable.run();

        verify(mTabCreator).createNewTab(any(), anyInt(), any());
        verify(mTabGroupModelFilter).createSingleTabGroup(mTab);
    }
}
