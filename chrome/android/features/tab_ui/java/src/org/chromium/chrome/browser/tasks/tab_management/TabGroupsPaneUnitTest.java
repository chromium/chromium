// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;

/** Unit tests for {@link TabGroupsPane}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
public class TabGroupsPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private DoubleConsumer mOnToolbarAlphaChange;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Supplier<PaneManager> mPaneManagerSupplier;
    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock Supplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    @Mock FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock SyncService mSyncService;

    private final OneshotSupplierImpl<ProfileProvider> mProfileSupplier =
            new OneshotSupplierImpl<>();

    private TabGroupsPane mTabGroupsPane;

    @Before
    public void setUp() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        mJniMocker.mock(FaviconHelperJni.TEST_HOOKS, mFaviconHelperJniMock);
        ApplicationProvider.getApplicationContext().setTheme(R.style.Theme_BrowserUI_DayNight);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        mProfileSupplier.set(mProfileProvider);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});

        mTabGroupsPane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange,
                        mProfileSupplier,
                        mPaneManagerSupplier,
                        mTabGroupUiActionHandlerSupplier,
                        mModalDialogManagerSupplier);
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
        mTabGroupsPane.notifyLoadHint(LoadHint.HOT);
        assertNotEquals(0, mTabGroupsPane.getRootView().getChildCount());
    }
}
