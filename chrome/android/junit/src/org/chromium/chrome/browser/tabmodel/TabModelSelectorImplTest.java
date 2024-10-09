// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link TabModelSelectorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelSelectorImplTest {
    // Test activity type that does not restore tab on cold restart.
    // Any type other than ActivityType.TABBED works.
    private static final @ActivityType int NO_RESTORE_TYPE = ActivityType.CUSTOM_TAB;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabContentManager mMockTabContentManager;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private NextTabPolicySupplier mNextTabPolicySupplier;

    @Mock
    private IncognitoTabModelObserver.IncognitoReauthDialogDelegate
            mIncognitoReauthDialogDelegateMock;

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();

    @Mock private Callback<TabModel> mTabModelSupplierObserverMock;
    @Mock private Callback<Tab> mTabSupplierObserverMock;
    @Mock private Callback<Integer> mTabCountSupplierObserverMock;
    @Mock private TabModelSelectorObserver mTabModelSelectorObserverMock;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private Context mContext;

    private TabModelSelectorImpl mTabModelSelector;
    private MockTabCreatorManager mTabCreatorManager;
    private MockTabModel mRegularTabModel;
    private MockTabModel mIncognitoTabModel;
    private AsyncTabParamsManager mAsyncTabParamsManager;

    @Before
    public void setUp() {
        doReturn(true).when(mIncognitoProfile).isOffTheRecord();
        mTabCreatorManager = new MockTabCreatorManager();

        mAsyncTabParamsManager = AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        mProfileProviderSupplier.set(mProfileProvider);
        mTabModelSelector =
                new TabModelSelectorImpl(
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mAsyncTabParamsManager,
                        /* supportUndo= */ false,
                        NO_RESTORE_TYPE,
                        /* startIncognito= */ false);

        mRegularTabModel = new MockTabModel(mProfile, null);
        mRegularTabModel.setActive(true);
        mIncognitoTabModel = new MockTabModel(mIncognitoProfile, null);

        assertTrue(currentTabModelSupplierHasObservers());
        assertNull(mTabModelSelector.getCurrentTabModelSupplier().get());
        assertNull(mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter());

        mTabCreatorManager.initialize(mTabModelSelector);
        mTabModelSelector.onNativeLibraryReadyInternal(
                mMockTabContentManager, mRegularTabModel, mIncognitoTabModel);

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

        MockTab incognitoTab = new MockTab(2, mIncognitoProfile);
        mTabModelSelector
                .getModel(true)
                .addTab(
                        incognitoTab,
                        0,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector.getModel(true).setIndex(0, TabSelectionType.FROM_USER);
        assertEquals(normalTab, mTabModelSelector.getCurrentTabSupplier().get());
        assertEquals(
                mTabModelSelector.getModel(false),
                mTabModelSelector
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getTabModel());

        mTabModelSelector.selectModel(true);
        assertEquals(incognitoTab, mTabModelSelector.getCurrentTabSupplier().get());
        assertEquals(
                mTabModelSelector.getModel(true),
                mTabModelSelector
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getTabModel());
        ShadowLooper.runUiThreadTasks();
        verify(mTabSupplierObserverMock).onResult(eq(incognitoTab));

        mTabModelSelector.selectModel(false);
        assertEquals(normalTab, mTabModelSelector.getCurrentTabSupplier().get());
        assertEquals(
                mTabModelSelector.getModel(false),
                mTabModelSelector
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getTabModel());
        ShadowLooper.runUiThreadTasks();
        verify(mTabSupplierObserverMock, times(2)).onResult(eq(normalTab));

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

        MockTab incognitoTab = new MockTab(2, mIncognitoProfile);
        mTabModelSelector
                .getModel(true)
                .addTab(
                        incognitoTab,
                        0,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND);
        ShadowLooper.runUiThreadTasks();
        verify(mTabCountSupplierObserverMock).onResult(2);
        assertEquals(2, mTabModelSelector.getCurrentModelTabCountSupplier().get().intValue());

        mTabModelSelector.selectModel(true);
        ShadowLooper.runUiThreadTasks();
        verify(mTabCountSupplierObserverMock, times(2)).onResult(1);
        assertEquals(1, mTabModelSelector.getCurrentModelTabCountSupplier().get().intValue());

        mTabModelSelector.getModel(false).removeTab(normalTab1);
        mTabModelSelector.getModel(false).removeTab(normalTab2);
        assertEquals(1, mTabModelSelector.getCurrentModelTabCountSupplier().get().intValue());
        verify(mTabCountSupplierObserverMock, times(2)).onResult(1);

        mTabModelSelector.selectModel(false);
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

    /**
     * A test method to verify that {@link
     * IncognitoReauthDialogDelegate#OnBeforeIncognitoTabModelSelected} gets called before any other
     * {@link TabModelSelectorObserver} listening to {@link
     * TabModelSelectorObserver#onTabModelSelected}.
     */
    @Test
    public void
            testIncognitoReauthDialogDelegate_OnBeforeIncognitoTabModelSelected_called_Before() {
        doNothing().when(mIncognitoReauthDialogDelegateMock).onBeforeIncognitoTabModelSelected();
        doNothing().when(mTabModelSelectorObserverMock).onTabModelSelected(any(), any());
        doNothing().when(mTabModelSupplierObserverMock).onResult(any());
        mTabModelSelector.setIncognitoReauthDialogDelegate(mIncognitoReauthDialogDelegateMock);
        mTabModelSelector.addObserver(mTabModelSelectorObserverMock);
        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mTabModelSupplierObserverMock);
        ShadowLooper.runUiThreadTasks();
        verify(mTabModelSupplierObserverMock).onResult(any());

        InOrder order =
                inOrder(
                        mIncognitoReauthDialogDelegateMock,
                        mTabModelSelectorObserverMock,
                        mTabModelSupplierObserverMock);
        mTabModelSelector.selectModel(/* incognito= */ true);

        order.verify(mIncognitoReauthDialogDelegateMock).onBeforeIncognitoTabModelSelected();
        order.verify(mTabModelSupplierObserverMock).onResult(any());
        order.verify(mTabModelSelectorObserverMock).onTabModelSelected(any(), any());

        mTabModelSelector
                .getCurrentTabModelSupplier()
                .removeObserver(mTabModelSupplierObserverMock);
    }

    /**
     * A test method to verify that {@link
     * IncognitoReauthDialogDelegate#onAfterRegularTabModelChanged} gets called after any other
     * {@link TabModelSelectorObserver} listening to {@link TabModelSelectorObserver#onChange()}.
     */
    @Test
    public void testIncognitoReauthDialogDelegate_onAfterRegularTabModelChanged() {
        // Start-off with an Incognito tab model. This is needed to set up the environment.
        mTabModelSelector.selectModel(/* incognito= */ true);
        // The above calls posts a tasks which can get executed after we add
        // mTabModelSelectorObserverMock below and interfering with the verify onChange test below.
        // Therefore execute that task immediately now.
        ShadowLooper.shadowMainLooper().idle();
        // Add the observers now to prevent any firing from the previous selectModel which is
        // separate from the actual test.
        mTabModelSelector.setIncognitoReauthDialogDelegate(mIncognitoReauthDialogDelegateMock);
        mTabModelSelector.addObserver(mTabModelSelectorObserverMock);
        doNothing().when(mTabModelSupplierObserverMock).onResult(any());
        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mTabModelSupplierObserverMock);
        ShadowLooper.runUiThreadTasks();
        verify(mTabModelSupplierObserverMock).onResult(any());

        doNothing().when(mIncognitoReauthDialogDelegateMock).onAfterRegularTabModelChanged();
        doNothing().when(mTabModelSelectorObserverMock).onTabModelSelected(any(), any());
        doNothing().when(mTabModelSelectorObserverMock).onChange();

        InOrder order = inOrder(mTabModelSelectorObserverMock, mIncognitoReauthDialogDelegateMock);
        mTabModelSelector.selectModel(/* incognito= */ false);
        verify(mTabModelSelectorObserverMock).onTabModelSelected(any(), any());
        verify(mTabModelSupplierObserverMock, times(2)).onResult(any());

        // The onChange method below is posted as a task to the main looper, and therefore we need
        // to wait until it gets executed.
        ShadowLooper.shadowMainLooper().idle();
        order.verify(mTabModelSelectorObserverMock).onChange();
        order.verify(mIncognitoReauthDialogDelegateMock).onAfterRegularTabModelChanged();

        mTabModelSelector
                .getCurrentTabModelSupplier()
                .removeObserver(mTabModelSupplierObserverMock);
    }

    @Test
    public void testOnActivityAttachmentChanged() {
        MockTab tab0 = mRegularTabModel.addTab(0);
        MockTab tab1 = mRegularTabModel.addTab(1);
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false);
        assertEquals(filter.getTabModel(), mRegularTabModel);
        assertEquals(0, TabModelUtils.getTabIndexById(mRegularTabModel, tab0.getId()));
        assertEquals(1, TabModelUtils.getTabIndexById(mRegularTabModel, tab1.getId()));

        assertFalse(filter.isTabInTabGroup(tab1));
        for (TabObserver observer : tab1.getObservers()) {
            observer.onActivityAttachmentChanged(tab1, /* window= */ null);
        }
        assertFalse(filter.isTabInTabGroup(tab1));
        assertEquals(0, TabModelUtils.getTabIndexById(mRegularTabModel, tab0.getId()));
        assertEquals(
                TabModel.INVALID_TAB_INDEX,
                TabModelUtils.getTabIndexById(mRegularTabModel, tab1.getId()));

        filter.createSingleTabGroup(tab0, /* notify= */ true);

        assertTrue(filter.isTabInTabGroup(tab0));
        for (TabObserver observer : tab0.getObservers()) {
            observer.onActivityAttachmentChanged(tab0, /* window= */ null);
        }
        assertFalse(filter.isTabInTabGroup(tab0));
        assertNull(tab0.getTabGroupId());
        assertEquals(tab0.getId(), tab0.getRootId());
        assertEquals(
                TabModel.INVALID_TAB_INDEX,
                TabModelUtils.getTabIndexById(mRegularTabModel, tab0.getId()));
    }

    @Test
    public void testMarkTabStateInitializedReentrancy() {
        mTabModelSelector.destroy();

        TabModelImpl regularModel = mock(TabModelImpl.class);
        mTabModelSelector =
                new TabModelSelectorImpl(
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mAsyncTabParamsManager,
                        /* supportUndo= */ false,
                        NO_RESTORE_TYPE,
                        /* startIncognito= */ false);
        when(regularModel.isActiveModel()).thenReturn(true);
        mTabModelSelector.initializeForTesting(regularModel, mIncognitoTabModel);
        TabModelSelectorObserver observer =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabStateInitialized() {
                        verify(regularModel, never()).broadcastSessionRestoreComplete();
                        mTabModelSelector.markTabStateInitialized();

                        // Should not be called due to re-entrancy guard until this observer
                        // returns.
                        verify(regularModel, never()).broadcastSessionRestoreComplete();
                    }
                };

        mTabModelSelector.addObserver(observer);

        mTabModelSelector.markTabStateInitialized();

        mTabModelSelector.removeObserver(observer);

        // Should be called exactly once.
        verify(regularModel).broadcastSessionRestoreComplete();
    }

    private boolean currentTabModelSupplierHasObservers() {
        return ((ObservableSupplierImpl<?>) mTabModelSelector.getCurrentTabModelSupplier())
                .hasObservers();
    }
}
