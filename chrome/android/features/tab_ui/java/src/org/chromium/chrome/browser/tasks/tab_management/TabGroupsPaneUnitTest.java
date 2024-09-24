// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;

/** Unit tests for {@link TabGroupsPane}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID, ChromeFeatureList.DATA_SHARING})
public class TabGroupsPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private DoubleConsumer mOnToolbarAlphaChange;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Supplier<PaneManager> mPaneManagerSupplier;
    @Mock Supplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    @Mock FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock SyncService mSyncService;
    @Mock ModalDialogManager mModalDialogManager;
    @Mock TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock EdgeToEdgeController mEdgeToEdgeController;

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
        mJniMocker.mock(FaviconHelperJni.TEST_HOOKS, mFaviconHelperJniMock);
        ApplicationProvider.getApplicationContext().setTheme(R.style.Theme_BrowserUI_DayNight);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        mProfileSupplier.set(mProfileProvider);
        mModalDialogManagerSupplier.set(mModalDialogManager);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        mJniMocker.mock(TabGroupSyncFeaturesJni.TEST_HOOKS, mTabGroupSyncFeaturesJniMock);
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);

        mTabGroupsPane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange,
                        mProfileSupplier,
                        mPaneManagerSupplier,
                        mTabGroupUiActionHandlerSupplier,
                        mModalDialogManagerSupplier,
                        mEdgeToEdgeSupplier);
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
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID)
    public void testWithoutSyncFeature() {
        doReturn(false).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);
        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        assertNotEquals(0, mTabGroupsPane.getRootView().getChildCount());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE
    })
    public void testEdgeToEdgePadAdjuster_BeforeLoadHint() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        assertFalse(mEdgeToEdgeSupplier.hasObservers());

        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        assertTrue(mEdgeToEdgeSupplier.hasObservers());
        ShadowLooper.idleMainLooper();
        verify(mEdgeToEdgeController).registerAdjuster(notNull());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE
    })
    public void testEdgeToEdgePadAdjuster_AfterLoadHint() {
        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        assertTrue(mEdgeToEdgeSupplier.hasObservers());

        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(notNull());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE
    })
    public void testEdgeToEdgePadAdjuster_ChangeController() {
        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(notNull());

        EdgeToEdgeController controller2 = Mockito.mock(EdgeToEdgeController.class);
        mEdgeToEdgeSupplier.set(controller2);
        verify(controller2).registerAdjuster(notNull());
        verify(mEdgeToEdgeController).unregisterAdjuster(notNull());
    }
}
