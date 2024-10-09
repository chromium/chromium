// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link TabModelSelectorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ArchivedTabModelSelectorImplTest {
    // Test activity type that does not restore tab on cold restart.
    // Any type other than ActivityType.TABBED works.
    private static final @ActivityType int NO_RESTORE_TYPE = ActivityType.CUSTOM_TAB;

    @Mock private TabContentManager mMockTabContentManager;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private NextTabPolicySupplier mNextTabPolicySupplier;

    @Mock
    private IncognitoTabModelObserver.IncognitoReauthDialogDelegate
            mIncognitoReauthDialogDelegateMock;

    @Mock private Callback<TabModel> mTabModelSupplierObserverMock;
    @Mock private Callback<Tab> mTabSupplierObserverMock;
    @Mock private Callback<Integer> mTabCountSupplierObserverMock;
    @Mock private TabModelSelectorObserver mTabModelSelectorObserverMock;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private Context mContext;

    private ArchivedTabModelSelectorImpl mTabModelSelector;
    private MockTabCreatorManager mTabCreatorManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(true).when(mIncognitoProfile).isOffTheRecord();
        mTabCreatorManager = new MockTabCreatorManager();

        AsyncTabParamsManager realAsyncTabParamsManager =
                AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        mTabModelSelector =
                new ArchivedTabModelSelectorImpl(
                        mProfile,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        realAsyncTabParamsManager);
        assertTrue(currentTabModelSupplierHasObservers());
        assertNull(mTabModelSelector.getCurrentTabModelSupplier().get());
        assertNull(mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter());

        mTabCreatorManager.initialize(mTabModelSelector);
        mTabModelSelector.onNativeLibraryReadyInternal(
                mMockTabContentManager,
                new MockTabModel(mProfile, null),
                new MockTabModel(mIncognitoProfile, null));

        assertEquals(
                mTabModelSelector.getModel(/* isIncognito= */ false),
                mTabModelSelector.getCurrentTabModelSupplier().get());
        assertEquals(
                mTabModelSelector.getCurrentModel(),
                mTabModelSelector.getCurrentTabModelSupplier().get());
        assertEquals(
                mTabModelSelector.getCurrentModel(),
                mTabModelSelector
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getTabModel());
    }

    @After
    public void tearDown() {
        mTabModelSelector.destroy();
        assertFalse(currentTabModelSupplierHasObservers());
    }

    @Test
    public void testCurrentTabSupplier() {
        mTabModelSelector.getCurrentTabSupplier().addObserver(mTabSupplierObserverMock);
        assertNull(mTabModelSelector.getCurrentTabSupplier().get());

        MockTab normalTab = new MockTab(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab,
                        0,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector.getModel(false).setIndex(0, TabSelectionType.FROM_USER);
        assertEquals(normalTab, mTabModelSelector.getModel(false).getCurrentTabSupplier().get());
        assertEquals(normalTab, mTabModelSelector.getCurrentTabSupplier().get());
        assertEquals(
                mTabModelSelector.getModel(false),
                mTabModelSelector
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getTabModel());
        ShadowLooper.runUiThreadTasks();
        verify(mTabSupplierObserverMock).onResult(eq(normalTab));
        mTabModelSelector.getCurrentTabSupplier().removeObserver(mTabSupplierObserverMock);
    }

    @Test
    public void testCurrentModelTabCountSupplier() {
        mTabModelSelector
                .getCurrentModelTabCountSupplier()
                .addObserver(mTabCountSupplierObserverMock);
        assertEquals(0, mTabModelSelector.getCurrentModelTabCountSupplier().get().intValue());
        ShadowLooper.runUiThreadTasks();
        verify(mTabCountSupplierObserverMock).onResult(0);

        MockTab normalTab1 = new MockTab(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab1,
                        0,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND);
        ShadowLooper.runUiThreadTasks();
        verify(mTabCountSupplierObserverMock).onResult(1);
        assertEquals(1, mTabModelSelector.getCurrentModelTabCountSupplier().get().intValue());

        MockTab normalTab2 = new MockTab(2, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab2,
                        0,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND);
        ShadowLooper.runUiThreadTasks();
        verify(mTabCountSupplierObserverMock).onResult(2);
        assertEquals(2, mTabModelSelector.getCurrentModelTabCountSupplier().get().intValue());

        mTabModelSelector.getModel(false).removeTab(normalTab1);
        mTabModelSelector.getModel(false).removeTab(normalTab2);
        ShadowLooper.runUiThreadTasks();
        assertEquals(0, mTabModelSelector.getCurrentModelTabCountSupplier().get().intValue());
        verify(mTabCountSupplierObserverMock, times(2)).onResult(0);

        mTabModelSelector
                .getCurrentModelTabCountSupplier()
                .removeObserver(mTabCountSupplierObserverMock);
    }

    @Test
    public void testTabActivityAttachmentChanged_detaching() {
        MockTab tab = new MockTab(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        tab.updateAttachment(null, null);

        Assert.assertEquals(
                "detaching a tab should result in it being removed from the model",
                0,
                mTabModelSelector.getModel(false).getCount());
    }

    @Test
    public void testTabActivityAttachmentChanged_movingWindows() {
        MockTab tab = new MockTab(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        WindowAndroid window = mock(WindowAndroid.class);
        WeakReference<Context> weakContext = new WeakReference<>(mContext);
        when(window.getContext()).thenReturn(weakContext);
        tab.updateAttachment(window, mTabDelegateFactory);

        Assert.assertEquals(
                "moving a tab between windows shouldn't remove it from the model",
                1,
                mTabModelSelector.getModel(false).getCount());
    }

    @Test
    public void testTabActivityAttachmentChanged_detachingWhileReparentingInProgress() {
        MockTab tab = new MockTab(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        mTabModelSelector.enterReparentingMode();
        tab.updateAttachment(null, null);

        Assert.assertEquals(
                "tab shouldn't be removed while reparenting is in progress",
                1,
                mTabModelSelector.getModel(false).getCount());
    }

    private boolean currentTabModelSupplierHasObservers() {
        return ((ObservableSupplierImpl<?>) mTabModelSelector.getCurrentTabModelSupplier())
                .hasObservers();
    }
}
