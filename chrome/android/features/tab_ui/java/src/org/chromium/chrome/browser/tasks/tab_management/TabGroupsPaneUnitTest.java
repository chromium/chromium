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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.function.DoubleConsumer;

/** Unit tests for {@link TabGroupsPane}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupsPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private DoubleConsumer mOnToolbarAlphaChange;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock FaviconHelper.Natives mFaviconHelperJniMock;

    private final OneshotSupplierImpl<ProfileProvider> mProfileSupplier =
            new OneshotSupplierImpl<>();

    @Before
    public void setUp() {
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        mJniMocker.mock(FaviconHelperJni.TEST_HOOKS, mFaviconHelperJniMock);
        ApplicationProvider.getApplicationContext().setTheme(R.style.Theme_BrowserUI_DayNight);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        mProfileSupplier.set(mProfileProvider);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
    }

    @Test
    public void testNotifyLoadHint() {
        TabGroupsPane pane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange,
                        mProfileSupplier);
        assertEquals(0, pane.getRootView().getChildCount());

        pane.notifyLoadHint(LoadHint.HOT);
        assertNotEquals(0, pane.getRootView().getChildCount());

        pane.notifyLoadHint(LoadHint.COLD);
        assertEquals(0, pane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileHot() {
        TabGroupsPane pane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange,
                        mProfileSupplier);
        pane.notifyLoadHint(LoadHint.HOT);
        pane.destroy();
        assertEquals(0, pane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileCold() {
        TabGroupsPane pane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange,
                        mProfileSupplier);
        pane.notifyLoadHint(LoadHint.HOT);
        pane.notifyLoadHint(LoadHint.COLD);
        pane.destroy();
        assertEquals(0, pane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_NoLoadHint() {
        TabGroupsPane pane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange,
                        mProfileSupplier);
        pane.destroy();
        assertEquals(0, pane.getRootView().getChildCount());
    }
}
