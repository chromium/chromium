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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;

import java.util.List;

/** Unit tests for {@link TabModelImpl}. */
// TODO(crbug.com/454344854): The remaining tests in this suite can be deleted along with
// TabModelImpl.
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelImplUnitTest {
    private static final long FAKE_NATIVE_ADDRESS = 123L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    /** Disable native calls from {@link TabModelJniBridge}. */
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

    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;

    @Mock private Callback<Tab> mTabSupplierObserver;
    @Mock private TabModel mEmptyModel;

    private int mNextTabId;

    @Before
    public void setUp() {
        // Disable HomepageManager#shouldCloseAppWithZeroTabs() for TabModelImpl#closeTabs().
        HomepageManager.getInstance().setJavaPrefHomepageEnabled(false);

        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(mIncognitoProfile.isIncognitoBranded()).thenReturn(true);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        TabModelJniBridgeJni.setInstanceForTesting(mTabModelJniBridge);
        when(mTabModelJniBridge.init(
                        any(TabModelJniBridge.class), any(Profile.class), anyInt(), anyBoolean()))
                .thenReturn(FAKE_NATIVE_ADDRESS);

        when(mTabModelDelegate.isReparentingInProgress()).thenReturn(false);
        when(mTabModelDelegate.getModel(anyBoolean())).thenReturn(mEmptyModel);

        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(true))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getValidPosition(any(), anyInt()))
                .thenAnswer(i -> i.getArguments()[1]);

        when(mTabGroupModelFilter.getValidPosition(any(), anyInt()))
                .thenAnswer(i -> i.getArguments()[1]);

        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);

        mNextTabId = 0;
    }

    private Tab createTab(final TabModel model) {
        return createTab(
                model, 0, Tab.INVALID_TAB_ID, TabList.INVALID_TAB_INDEX, /* isPinned= */ false);
    }

    private Tab createTab(
            final TabModel model,
            long activeTimestampMillis,
            int parentId,
            int tabIndex,
            boolean isPinned) {
        MockTab tab = MockTab.createAndInitialize(mNextTabId++, model.getProfile());
        tab.setTimestampMillis(activeTimestampMillis);
        tab.setParentId(parentId);
        tab.setIsInitialized(true);
        tab.setIsPinned(isPinned);
        model.addTab(
                tab, tabIndex, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        return tab;
    }

    private void selectTab(final TabModel model, final Tab tab) {
        model.setIndex(model.indexOf(tab), TabSelectionType.FROM_USER);
    }

    /** Create a {@link TabModel} to use for the test. */
    @SuppressWarnings("DirectInvocationOnMock")
    private TabModelImpl createTabModel(boolean isActive, boolean isIncognito) {
        AsyncTabParamsManager realAsyncTabParamsManager =
                AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        TabModelOrderControllerImpl orderController =
                new TabModelOrderControllerImpl(mTabModelSelector);
        Profile profile = isIncognito ? mIncognitoProfile : mProfile;
        TabRemover tabRemover =
                new TabRemover() {
                    @Override
                    public void closeTabs(
                            @NonNull TabClosureParams tabClosureParams,
                            boolean allowDialog,
                            @Nullable TabModelActionListener listener) {
                        forceCloseTabs(tabClosureParams);
                    }

                    @Override
                    public void prepareCloseTabs(
                            @NonNull TabClosureParams tabClosureParams,
                            boolean allowDialog,
                            @Nullable TabModelActionListener listener,
                            @NonNull Callback<TabClosureParams> onPreparedCallback) {
                        onPreparedCallback.onResult(tabClosureParams);
                    }

                    @Override
                    public void forceCloseTabs(@NonNull TabClosureParams tabClosureParams) {
                        ((TabModelImpl) mTabModelSelector.getModel(isIncognito))
                                .closeTabs(tabClosureParams);
                    }

                    @Override
                    public void removeTab(
                            @NonNull Tab tab,
                            boolean allowDialog,
                            @Nullable TabModelActionListener listener) {
                        ((TabModelImpl) mTabModelSelector.getModel(isIncognito)).removeTab(tab);
                    }
                };
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
                        tabRemover,
                        /* supportUndo= */ true,
                        /* isArchivedTabModel= */ false);
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
    public void testDontSwitchModelsIfIncognitoGroupClosed() {
        TabModel activeIncognito = createTabModel(true, true);
        TabModel inactiveNormal = createTabModel(false, false);

        Tab incognitoTab0 = createTab(activeIncognito);
        Tab incognitoTab1 = createTab(activeIncognito);
        Tab incognitoTab2 = createTab(activeIncognito);
        createTab(inactiveNormal);

        selectTab(activeIncognito, incognitoTab0);

        activeIncognito
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(List.of(incognitoTab0, incognitoTab1))
                                .allowUndo(false)
                                .build(),
                        /* allowDialog= */ false);
        verify(mTabModelSelector, never()).selectModel(anyBoolean());
        assertEquals(incognitoTab2, activeIncognito.getTabAt(activeIncognito.index()));
    }

    @Test
    public void testObserveCurrentTabSupplierActiveNormal() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        createTabModel(false, true);

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

        activeNormal.getTabRemover().removeTab(tab0, /* allowDialog= */ true);
        assertEquals(tab1, activeNormal.getCurrentTabSupplier().get());
        assertEquals(1, activeNormal.getTabCountSupplier().get().intValue());
        verify(mTabSupplierObserver, times(2)).onResult(eq(tab1));
    }

    @Test
    public void testObserveCurrentTabSupplierInactiveNormal() {
        TabModel inactiveNormal = createTabModel(false, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        createTabModel(true, true);

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

        inactiveNormal.getTabRemover().removeTab(tab0, /* allowDialog= */ true);
        assertEquals(tab1, inactiveNormal.getCurrentTabSupplier().get());
        assertEquals(1, inactiveNormal.getTabCountSupplier().get().intValue());
        verify(mTabSupplierObserver, times(2)).onResult(eq(tab1));
    }

    @Test
    public void testObserveTabCountSupplierActiveNormal() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        createTabModel(false, true);

        assertEquals(0, activeNormal.getTabCountSupplier().get().intValue());

        Tab tab0 = createTab(activeNormal);
        assertEquals(1, activeNormal.getTabCountSupplier().get().intValue());

        Tab tab1 = createTab(activeNormal);
        assertEquals(2, activeNormal.getTabCountSupplier().get().intValue());

        Tab tab2 = createTab(activeNormal);
        assertEquals(3, activeNormal.getTabCountSupplier().get().intValue());

        Tab tab3 = createTab(activeNormal);
        assertEquals(4, activeNormal.getTabCountSupplier().get().intValue());

        activeNormal
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(List.of(tab0, tab1)).allowUndo(false).build(),
                        /* allowDialog= */ false);
        assertEquals(2, activeNormal.getTabCountSupplier().get().intValue());

        activeNormal
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeAllTabs().allowUndo(false).build(),
                        /* allowDialog= */ false);
        assertEquals(0, activeNormal.getTabCountSupplier().get().intValue());
    }

    @Test
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
    @EnableFeatures({
        ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP,
        ChromeFeatureList.ANDROID_PINNED_TABS
    })
    public void testAddTab_PinnedTabIndex_OutOfRange() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        createTabModel(false, true);
        assertEquals(0, activeNormal.getTabCountSupplier().get().intValue());

        // Create first pinned tab.
        Tab tab0 = createTab(activeNormal);
        tab0.setIsPinned(true);
        assertEquals(1, activeNormal.getTabCountSupplier().get().intValue());
        assertEquals(0, activeNormal.indexOf(tab0));

        // Create second pinned tab.
        Tab tab1 = createTab(activeNormal);
        tab1.setIsPinned(true);
        assertEquals(2, activeNormal.getTabCountSupplier().get().intValue());
        assertEquals(1, activeNormal.indexOf(tab1));

        // Create first unpinned tab.
        Tab tab2 = createTab(activeNormal);
        assertEquals(3, activeNormal.getTabCountSupplier().get().intValue());
        assertEquals(2, activeNormal.indexOf(tab2));

        // Create second unpinned tab.
        Tab tab3 = createTab(activeNormal);
        assertEquals(4, activeNormal.getTabCountSupplier().get().intValue());
        assertEquals(3, activeNormal.indexOf(tab3));

        // Attempt to create a pinned tab at the last index (invalid); verify it lands at the first
        // non-pinned index, and that other tabs shift accordingly.
        Tab tab4 =
                createTab(
                        activeNormal,
                        /* activeTimestampMillis= */ 0,
                        /* parentId= */ Tab.INVALID_TAB_ID,
                        /* tabIndex= */ 4,
                        /* isPinned= */ true);
        assertEquals(5, activeNormal.getTabCountSupplier().get().intValue());
        assertEquals(2, activeNormal.indexOf(tab4));
        assertEquals(3, activeNormal.indexOf(tab2));
        assertEquals(4, activeNormal.indexOf(tab3));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP,
        ChromeFeatureList.ANDROID_PINNED_TABS
    })
    public void testAddTab_PinnedTabIndex_WithinValidRange() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        createTabModel(false, true);
        assertEquals(0, activeNormal.getTabCountSupplier().get().intValue());

        // Create first pinned tab.
        Tab tab0 = createTab(activeNormal);
        tab0.setIsPinned(true);
        assertEquals(1, activeNormal.getTabCountSupplier().get().intValue());
        assertEquals(0, activeNormal.indexOf(tab0));

        // Create second pinned tab.
        Tab tab1 = createTab(activeNormal);
        tab1.setIsPinned(true);
        assertEquals(2, activeNormal.getTabCountSupplier().get().intValue());
        assertEquals(1, activeNormal.indexOf(tab1));

        // Create first unpinned tab.
        Tab tab2 = createTab(activeNormal);
        tab2.setIsPinned(true);
        assertEquals(3, activeNormal.getTabCountSupplier().get().intValue());
        assertEquals(2, activeNormal.indexOf(tab2));

        // Create second unpinned tab.
        Tab tab3 = createTab(activeNormal);
        assertEquals(4, activeNormal.getTabCountSupplier().get().intValue());
        assertEquals(3, activeNormal.indexOf(tab3));

        // Create a new pinned tab at index 1 (valid) and verify its final index remains 0
        // (unchanged), with the other tabs shifting accordingly.
        Tab tab4 =
                createTab(
                        activeNormal,
                        /* activeTimestampMillis= */ 0,
                        /* parentId= */ Tab.INVALID_TAB_ID,
                        /* tabIndex= */ 1,
                        /* isPinned= */ true);
        assertEquals(5, activeNormal.getTabCountSupplier().get().intValue());
        assertEquals(1, activeNormal.indexOf(tab4));
        assertEquals(2, activeNormal.indexOf(tab1));
        assertEquals(3, activeNormal.indexOf(tab2));
        assertEquals(4, activeNormal.indexOf(tab3));
    }
}
