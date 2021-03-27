// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.ui.base.WindowAndroid;

/**
 * Unit tests for {@link TabModelSelectorImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelSelectorImplTest {
    // Test activity type that does not restore tab on cold restart.
    // Any type other than ActivityType.TABBED works.
    private static final @ActivityType int NO_RESTORE_TYPE = ActivityType.CUSTOM_TAB;

    @Mock
    TabModelFilterFactory mMockTabModelFilterFactory;
    @Mock
    TabContentManager mMockTabContentManager;
    @Mock
    TabDelegateFactory mTabDelegateFactory;
    @Mock
    NextTabPolicySupplier mNextTabPolicySupplier;

    private TabModelSelectorImpl mTabModelSelector;
    private MockTabCreatorManager mTabCreatorManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(mock(TabModelFilter.class))
                .when(mMockTabModelFilterFactory)
                .createTabModelFilter(any());
        mTabCreatorManager = new MockTabCreatorManager();

        AsyncTabParamsManager realAsyncTabParamsManager =
                AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        mTabModelSelector = new TabModelSelectorImpl(null, mTabCreatorManager,
                mMockTabModelFilterFactory, mNextTabPolicySupplier, realAsyncTabParamsManager,
                /*supportUndo=*/false, NO_RESTORE_TYPE, /*startIncognito=*/false);
        mTabCreatorManager.initialize(mTabModelSelector);
        mTabModelSelector.onNativeLibraryReadyInternal(mMockTabContentManager,
                new MockTabModel(false, null), new MockTabModel(true, null));
    }

    @Test
    public void testTabActivityAttachmentChanged_detaching() {
        MockTab tab = new MockTab(1, false);
        mTabModelSelector.getModel(false).addTab(
                tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        tab.updateAttachment(null, null);

        Assert.assertEquals("detaching a tab should result in it being removed from the model", 0,
                mTabModelSelector.getModel(false).getCount());
    }

    @Test
    public void testTabActivityAttachmentChanged_movingWindows() {
        MockTab tab = new MockTab(1, false);
        mTabModelSelector.getModel(false).addTab(
                tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        tab.updateAttachment(Mockito.mock(WindowAndroid.class), mTabDelegateFactory);

        Assert.assertEquals("moving a tab between windows shouldn't remove it from the model", 1,
                mTabModelSelector.getModel(false).getCount());
    }

    @Test
    public void testTabActivityAttachmentChanged_detachingWhileReparentingInProgress() {
        MockTab tab = new MockTab(1, false);
        mTabModelSelector.getModel(false).addTab(
                tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        mTabModelSelector.enterReparentingMode();
        tab.updateAttachment(null, null);

        Assert.assertEquals("tab shouldn't be removed while reparenting is in progress", 1,
                mTabModelSelector.getModel(false).getCount());
    }
}
