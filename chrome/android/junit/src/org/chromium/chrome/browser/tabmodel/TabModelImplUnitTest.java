// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link TabModelImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelImplUnitTest {
    private static final long FAKE_NATIVE_ADDRESS = 123L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    /** Disable native calls from {@link TabModelJniBridge}. */
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private TabModelJniBridge.Natives mTabModelJniBridge;

    /** Required to be non-null for {@link TabModelJniBridge}. */
    @Mock private Profile mProfile;

    @Mock private Profile mIncognitoProfile;

    /** Required to simulate tab thumbnail deletion. */
    @Mock private TabContentManager mTabContentManager;

    /** Required to handle some tab lookup actions. */
    @Mock private TabModelDelegate mTabModelDelegate;

    /** Required to handle some actions and initialize {@link TabModelOrderControllerImpl}. */
    @Mock private TabModelSelector mTabModelSelector;

    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabModelFilter mTabModelFilter;

    @Mock private Callback<Tab> mTabSupplierObserver;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private WeakReference<Context> mWeakReferenceContext;
    @Mock private WeakReference<Activity> mWeakReferenceActivity;

    private int mNextTabId;

    @Before
    public void setUp() {
        // Disable HomepageManager#shouldCloseAppWithZeroTabs() for TabModelImpl#closeTabs().
        HomepageManager.getInstance().setPrefHomepageEnabled(false);

        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        mJniMocker.mock(TabModelJniBridgeJni.TEST_HOOKS, mTabModelJniBridge);
        when(mTabModelJniBridge.init(any(), any(), anyInt(), anyBoolean()))
                .thenReturn(FAKE_NATIVE_ADDRESS);

        when(mTabModelDelegate.isReparentingInProgress()).thenReturn(false);

        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mTabModelFilter);
        when(mTabModelFilter.getValidPosition(any(), anyInt()))
                .thenAnswer(i -> i.getArguments()[1]);

        when(mWindowAndroid.getActivity()).thenReturn(mWeakReferenceActivity);
        when(mWindowAndroid.getContext()).thenReturn(mWeakReferenceContext);
        when(mTabGroupModelFilter.getValidPosition(any(), anyInt()))
                .thenAnswer(i -> i.getArguments()[1]);

        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);

        mNextTabId = 0;
    }

    private Tab createTab(final TabModel model) {
        return createTab(model, 0, Tab.INVALID_TAB_ID);
    }

    private Tab createTab(final TabModel model, long activeTimestampMillis, int parentId) {
        MockTab tab = MockTab.createAndInitialize(mNextTabId++, model.getProfile());
        tab.setTimestampMillis(activeTimestampMillis);
        tab.setParentId(parentId);
        tab.setIsInitialized(true);
        model.addTab(
                tab,
                TabList.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        return tab;
    }

    private void selectTab(final TabModel model, final Tab tab) {
        model.setIndex(model.indexOf(tab), TabSelectionType.FROM_USER);
    }

    /** Create a {@link TabModel} to use for the test. */
    private TabModelImpl createTabModel(boolean isActive, boolean isIncognito) {
        AsyncTabParamsManager realAsyncTabParamsManager =
                AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        TabModelOrderControllerImpl orderController =
                new TabModelOrderControllerImpl(mTabModelSelector);
        Profile profile = isIncognito ? mIncognitoProfile : mProfile;
        TabModelImpl tabModel =
                new TabModelImpl(
                        profile,
                        ActivityType.TABBED,
                        /* regularTabCreator= */ null,
                        /* incognitoTabCreator= */ null,
                        orderController,
                        mTabContentManager,
                        () -> NextTabPolicy.HIERARCHICAL,
                        realAsyncTabParamsManager,
                        mTabModelDelegate,
                        /* supportUndo= */ true,
                        /* trackInNativeModelList= */ true);
        when(mTabModelSelector.getModel(isIncognito)).thenReturn(tabModel);
        tabModel.setActive(isActive);
        if (isActive) {
            when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);
            when(mTabModelDelegate.getCurrentModel()).thenReturn(tabModel);
        }
        when(mTabModelDelegate.getModel(isIncognito)).thenReturn(tabModel);
        return tabModel;
    }

    @Test
    @SmallTest
    public void testGetNextTabIfClosed_InactiveModel() {
        TabModel activeIncognito = createTabModel(true, true);
        TabModel inactiveNormal = createTabModel(false, false);

        Tab incognitoTab0 = createTab(activeIncognito);
        Tab incognitoTab1 = createTab(activeIncognito);
        Tab tab0 = createTab(inactiveNormal);
        Tab tab1 = createTab(inactiveNormal);

        selectTab(activeIncognito, incognitoTab1);
        selectTab(inactiveNormal, tab1);

        assertEquals(incognitoTab1, inactiveNormal.getNextTabIfClosed(tab1.getId(), false));
    }

    @Test
    @SmallTest
    public void testGetNextTabIfClosed_NotCurrentTab() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        TabModel inactiveIncognito = createTabModel(false, true);

        Tab tab0 = createTab(activeNormal);
        Tab tab1 = createTab(activeNormal);
        Tab tab2 = createTab(activeNormal);

        selectTab(activeNormal, tab0);
        assertEquals(tab0, activeNormal.getNextTabIfClosed(tab1.getId(), false));
        assertEquals(tab0, activeNormal.getNextTabIfClosed(tab2.getId(), false));

        selectTab(activeNormal, tab1);
        assertEquals(tab1, activeNormal.getNextTabIfClosed(tab0.getId(), false));
        assertEquals(tab1, activeNormal.getNextTabIfClosed(tab2.getId(), false));

        selectTab(activeNormal, tab2);
        assertEquals(tab2, activeNormal.getNextTabIfClosed(tab0.getId(), false));
        assertEquals(tab2, activeNormal.getNextTabIfClosed(tab1.getId(), false));
    }

    @Test
    @SmallTest
    public void testGetNextTabIfClosed_ParentTab() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        TabModel inactiveIncognito = createTabModel(false, true);

        Tab tab0 = createTab(activeNormal);
        Tab tab1 = createTab(activeNormal);
        Tab tab2 = createTab(activeNormal, 0, tab0.getId());

        selectTab(activeNormal, tab2);
        assertEquals(tab0, activeNormal.getNextTabIfClosed(tab2.getId(), false));
    }

    @Test
    @SmallTest
    public void testGetNextTabIfClosed_Adjacent() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        TabModel inactiveIncognito = createTabModel(false, true);

        Tab tab0 = createTab(activeNormal);
        Tab tab1 = createTab(activeNormal);
        Tab tab2 = createTab(activeNormal);

        selectTab(activeNormal, tab0);
        assertEquals(tab1, activeNormal.getNextTabIfClosed(tab0.getId(), false));

        selectTab(activeNormal, tab1);
        assertEquals(tab0, activeNormal.getNextTabIfClosed(tab1.getId(), false));

        selectTab(activeNormal, tab2);
        assertEquals(tab1, activeNormal.getNextTabIfClosed(tab2.getId(), false));
    }

    @Test
    @SmallTest
    public void testGetNextTabIfClosed_LastIncognitoTab() {
        TabModel activeIncognito = createTabModel(true, true);
        TabModel inactiveNormal = createTabModel(false, false);

        Tab incognitoTab0 = createTab(activeIncognito);
        Tab tab0 = createTab(inactiveNormal);
        Tab tab1 = createTab(inactiveNormal);

        selectTab(inactiveNormal, tab0);
        assertEquals(tab0, activeIncognito.getNextTabIfClosed(incognitoTab0.getId(), false));

        selectTab(inactiveNormal, tab1);
        assertEquals(tab1, activeIncognito.getNextTabIfClosed(incognitoTab0.getId(), false));
    }

    @Test
    @SmallTest
    public void testGetNextTabIfClosed_MostRecentTab() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        TabModel inactiveIncognito = createTabModel(false, true);

        // uponExit overrides parent selection..
        Tab tab0 = createTab(activeNormal, 10, Tab.INVALID_TAB_ID);
        Tab tab1 = createTab(activeNormal, 200, tab0.getId());
        Tab tab2 = createTab(activeNormal, 30, tab0.getId());

        selectTab(activeNormal, tab0);
        assertEquals(tab1, activeNormal.getNextTabIfClosed(tab0.getId(), true));

        selectTab(activeNormal, tab1);
        assertEquals(tab2, activeNormal.getNextTabIfClosed(tab1.getId(), true));

        selectTab(activeNormal, tab2);
        assertEquals(tab1, activeNormal.getNextTabIfClosed(tab2.getId(), true));
    }

    @Test
    @SmallTest
    public void testGetNextTabIfClosed_InvalidSelection() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        TabModel inactiveIncognito = createTabModel(false, true);

        Tab tab0 = createTab(activeNormal);
        selectTab(activeNormal, tab0);
        assertNull(activeNormal.getNextTabIfClosed(tab0.getId(), false));
    }

    @Test
    @SmallTest
    public void testDontSwitchModelsIfIncognitoGroupClosed() {
        TabModel activeIncognito = createTabModel(true, true);
        TabModel inactiveNormal = createTabModel(false, false);

        Tab incognitoTab0 = createTab(activeIncognito);
        Tab incognitoTab1 = createTab(activeIncognito);
        Tab incognitoTab2 = createTab(activeIncognito);
        Tab tab0 = createTab(inactiveNormal);

        selectTab(activeIncognito, incognitoTab0);

        activeIncognito.closeTabs(
                TabClosureParams.closeTabs(List.of(incognitoTab0, incognitoTab1))
                        .allowUndo(false)
                        .build());
        verify(mTabModelSelector, never()).selectModel(anyBoolean());
        assertEquals(incognitoTab2, activeIncognito.getTabAt(activeIncognito.index()));
    }

    @Test
    @SmallTest
    public void testObserveCurrentTabSupplierActiveNormal() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        TabModel inactiveIncognito = createTabModel(false, true);

        assertNull(activeNormal.getCurrentTabSupplier().get());
        assertEquals(0, activeNormal.getTabCountSupplier().get().intValue());
        activeNormal.getCurrentTabSupplier().addObserver(mTabSupplierObserver);

        Tab tab0 = createTab(activeNormal);
        assertEquals(tab0, activeNormal.getCurrentTabSupplier().get());
        assertEquals(1, activeNormal.getTabCountSupplier().get().intValue());
        verify(mTabSupplierObserver).onResult(eq(tab0));

        Tab tab1 = createTab(activeNormal);
        assertEquals(tab1, activeNormal.getCurrentTabSupplier().get());
        assertEquals(2, activeNormal.getTabCountSupplier().get().intValue());
        verify(mTabSupplierObserver).onResult(eq(tab1));

        selectTab(activeNormal, tab0);
        assertEquals(tab0, activeNormal.getCurrentTabSupplier().get());
        assertEquals(2, activeNormal.getTabCountSupplier().get().intValue());
        verify(mTabSupplierObserver, times(2)).onResult(eq(tab0));

        activeNormal.removeTab(tab0);
        assertEquals(tab1, activeNormal.getCurrentTabSupplier().get());
        assertEquals(1, activeNormal.getTabCountSupplier().get().intValue());
        verify(mTabSupplierObserver, times(2)).onResult(eq(tab1));
    }

    @Test
    @SmallTest
    public void testObserveCurrentTabSupplierInactiveNormal() {
        TabModel inactiveNormal = createTabModel(false, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        TabModel activeIncognito = createTabModel(true, true);

        assertNull(inactiveNormal.getCurrentTabSupplier().get());
        assertEquals(0, inactiveNormal.getTabCountSupplier().get().intValue());
        inactiveNormal.getCurrentTabSupplier().addObserver(mTabSupplierObserver);

        Tab tab0 = createTab(inactiveNormal);
        assertEquals(tab0, inactiveNormal.getCurrentTabSupplier().get());
        assertEquals(1, inactiveNormal.getTabCountSupplier().get().intValue());
        verify(mTabSupplierObserver).onResult(eq(tab0));

        Tab tab1 = createTab(inactiveNormal);
        assertEquals(tab1, inactiveNormal.getCurrentTabSupplier().get());
        assertEquals(2, inactiveNormal.getTabCountSupplier().get().intValue());
        verify(mTabSupplierObserver).onResult(eq(tab1));

        selectTab(inactiveNormal, tab0);
        assertEquals(tab0, inactiveNormal.getCurrentTabSupplier().get());
        assertEquals(2, inactiveNormal.getTabCountSupplier().get().intValue());
        verify(mTabSupplierObserver, times(2)).onResult(eq(tab0));

        inactiveNormal.removeTab(tab0);
        assertEquals(tab1, inactiveNormal.getCurrentTabSupplier().get());
        assertEquals(1, inactiveNormal.getTabCountSupplier().get().intValue());
        verify(mTabSupplierObserver, times(2)).onResult(eq(tab1));
    }

    @Test
    @SmallTest
    public void testGetTabById() {
        TabModelImpl tabModel = createTabModel(/* isActive= */ true, /* isIncognito= */ false);
        createTabModel(/* isActive= */ false, /* isIncognito= */ true);

        Tab tab1 = createTab(tabModel);
        assertEquals(tab1, tabModel.getTabById(tab1.getId()));

        tabModel.closeTabs(TabClosureParams.closeTab(tab1).build());
        assertEquals(null, tabModel.getTabById(tab1.getId()));

        tabModel.cancelTabClosure(tab1.getId());
        assertEquals(tab1, tabModel.getTabById(tab1.getId()));

        tabModel.destroy();
        assertEquals(null, tabModel.getTabById(tab1.getId()));
    }

    @Test
    @SmallTest
    public void testGetTabsNavigatedInTimeWindow() {
        TabModelImpl tabModel = createTabModel(/* isActive= */ true, /* isIncognito= */ false);
        MockTab tab1 = (MockTab) createTab(tabModel, 0, Tab.INVALID_TAB_ID);
        tab1.setLastNavigationCommittedTimestampMillis(200);

        MockTab tab2 = (MockTab) createTab(tabModel, 0, Tab.INVALID_TAB_ID);
        tab2.setLastNavigationCommittedTimestampMillis(50);

        MockTab tab3 = (MockTab) createTab(tabModel, 0, Tab.INVALID_TAB_ID);
        tab3.setLastNavigationCommittedTimestampMillis(100);

        MockTab tab4 = (MockTab) createTab(tabModel, 0, Tab.INVALID_TAB_ID);
        tab4.setLastNavigationCommittedTimestampMillis(30);
        tab4.setIsCustomTab(true);

        MockTab tab5 = (MockTab) createTab(tabModel, 0, Tab.INVALID_TAB_ID);
        tab5.setLastNavigationCommittedTimestampMillis(10);

        assertEquals(Arrays.asList(tab2, tab5), tabModel.getTabsNavigatedInTimeWindow(10, 100));
    }

    @Test
    @SmallTest
    public void testCloseTabsNavigatedInTimeWindow() {
        when(mTabModelFilterProvider.getTabModelFilter(/* isIncognito= */ false))
                .thenReturn(mTabGroupModelFilter);

        TabModelImpl tabModel = createTabModel(/* isActive= */ true, /* isIncognito= */ false);

        MockTab tab1 = (MockTab) createTab(tabModel, 0, Tab.INVALID_TAB_ID);
        tab1.setLastNavigationCommittedTimestampMillis(200);
        tab1.updateAttachment(mWindowAndroid, mTabDelegateFactory);

        MockTab tab2 = (MockTab) createTab(tabModel, 0, Tab.INVALID_TAB_ID);
        tab2.setLastNavigationCommittedTimestampMillis(30);
        tab2.updateAttachment(mWindowAndroid, mTabDelegateFactory);

        MockTab tab3 = (MockTab) createTab(tabModel, 0, Tab.INVALID_TAB_ID);
        tab3.setLastNavigationCommittedTimestampMillis(20);
        tab3.updateAttachment(mWindowAndroid, mTabDelegateFactory);

        tabModel.closeTabsNavigatedInTimeWindow(20, 50);
        verify(mTabGroupModelFilter)
                .closeTabs(
                        TabClosureParams.closeTabs(Arrays.asList(tab2, tab3))
                                .allowUndo(false)
                                .saveToTabRestoreService(false)
                                .build());
    }
}
