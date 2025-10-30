// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

/** Tests for the TabModelSelectorTabRegistrationObserver. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelSelectorTabRegistrationObserverUnitTest {
    private static final long FAKE_NATIVE_ADDRESS = 123L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModelJniBridge.Natives mTabModelJniBridge;
    @Mock private TabCreatorManager mTabCreatorManager;

    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;

    private TabModelSelector mTabModelSelector;
    private TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;

    @Before
    public void setUp() {
        TabModelJniBridgeJni.setInstanceForTesting(mTabModelJniBridge);
        when(mTabModelJniBridge.init(
                        any(TabModelJniBridge.class), any(Profile.class), anyInt(), anyBoolean()))
                .thenReturn(FAKE_NATIVE_ADDRESS);

        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(false);

        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        mTabModelSelector = createTabModelSelector();
        mTabRegistrationObserver = new TabModelSelectorTabRegistrationObserver(mTabModelSelector);
    }

    private TabRemover createTabRemover(MockTabModel tabModel) {
        return new TabRemover() {
            @Override
            public void closeTabs(
                    TabClosureParams tabClosureParams,
                    boolean allowDialog,
                    @Nullable TabModelActionListener listener) {
                forceCloseTabs(tabClosureParams);
            }

            @Override
            public void prepareCloseTabs(
                    TabClosureParams tabClosureParams,
                    boolean allowDialog,
                    @Nullable TabModelActionListener listener,
                    Callback<TabClosureParams> onPreparedCallback) {
                fail("Not reached");
            }

            @Override
            public void forceCloseTabs(TabClosureParams tabClosureParams) {
                for (Tab tab : tabClosureParams.tabs) {
                    tabModel.removeTab(tab);
                }
            }

            @Override
            public void removeTab(Tab tab, boolean allowDialog, TabModelActionListener listener) {
                tabModel.removeTab(tab);
            }
        };
    }

    private TabModelSelector createTabModelSelector() {
        TestTabModelSelector selector = new TestTabModelSelector(mTabCreatorManager);
        TabModelOrderControllerImpl orderController = new TabModelOrderControllerImpl(selector);

        AsyncTabParamsManager realAsyncTabParamsManager =
                AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        NextTabPolicy.NextTabPolicySupplier nextTabPolicySupplier =
                () -> NextTabPolicy.HIERARCHICAL;

        MockTabModel normalTabModel = new MockTabModel(mProfile, null);
        normalTabModel.setTabRemoverForTesting(createTabRemover(normalTabModel));

        MockTabModel incognitoTabModel = new MockTabModel(mIncognitoProfile, null);
        incognitoTabModel.setTabRemoverForTesting(createTabRemover(incognitoTabModel));

        selector.initialize(
                TabModelHolderFactory.createTabModelHolderForTesting(normalTabModel),
                TabModelHolderFactory.createIncognitoTabModelHolderForTesting(incognitoTabModel));

        return selector;
    }

    @Test
    public void testOnTabRegistered_NewlyAddedTabs() {
        TabModelSelectorTabRegistrationObserver.Observer observer =
                mock(TabModelSelectorTabRegistrationObserver.Observer.class);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(observer);

        Tab normalTab1 = MockTab.createAndInitialize(1, mProfile);
        Tab normalTab2 = MockTab.createAndInitialize(2, mProfile);
        Tab incognitoTab1 = MockTab.createAndInitialize(3, mIncognitoProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab1,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab2,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector
                .getModel(true)
                .addTab(
                        incognitoTab1,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);

        verify(observer).onTabRegistered(normalTab1);
        verify(observer).onTabRegistered(normalTab2);
        verify(observer).onTabRegistered(incognitoTab1);
    }

    @Test
    public void testOnTabRegistered_ExistingTabs() {
        Tab normalTab1 = MockTab.createAndInitialize(1, mProfile);
        Tab normalTab2 = MockTab.createAndInitialize(2, mProfile);
        Tab incognitoTab1 = MockTab.createAndInitialize(3, mIncognitoProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab1,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab2,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector
                .getModel(true)
                .addTab(
                        incognitoTab1,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);

        TabModelSelectorTabRegistrationObserver.Observer observer =
                mock(TabModelSelectorTabRegistrationObserver.Observer.class);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(observer);

        verify(observer).onTabRegistered(normalTab1);
        verify(observer).onTabRegistered(normalTab2);
        verify(observer).onTabRegistered(incognitoTab1);
    }

    @Test
    public void testOnTabRegistered_NotCalledForPreviouslyRemovedTabs() {
        Tab normalTab1 = MockTab.createAndInitialize(1, mProfile);
        Tab normalTab2 = MockTab.createAndInitialize(2, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab1,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab2,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector
                .getModel(false)
                .getTabRemover()
                .removeTab(normalTab1, /* allowDialog= */ false);

        TabModelSelectorTabRegistrationObserver.Observer observer =
                mock(TabModelSelectorTabRegistrationObserver.Observer.class);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(observer);
        verify(observer).onTabRegistered(normalTab2);
        verifyNoMoreInteractions(observer);
    }

    @Test
    public void testOnTabUnRegistered_ExistingTab() {
        Tab normalTab1 = MockTab.createAndInitialize(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab1,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);

        TabModelSelectorTabRegistrationObserver.Observer observer =
                mock(TabModelSelectorTabRegistrationObserver.Observer.class);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(observer);

        verify(observer).onTabRegistered(normalTab1);

        mTabModelSelector
                .getModel(false)
                .getTabRemover()
                .removeTab(normalTab1, /* allowDialog= */ false);

        verify(observer).onTabUnregistered(normalTab1);
    }

    @Test
    public void testOnTabUnRegistered_NewAddedTab() {
        TabModelSelectorTabRegistrationObserver.Observer observer =
                mock(TabModelSelectorTabRegistrationObserver.Observer.class);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(observer);

        Tab normalTab1 = MockTab.createAndInitialize(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab1,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        verify(observer).onTabRegistered(normalTab1);

        mTabModelSelector
                .getModel(false)
                .getTabRemover()
                .removeTab(normalTab1, /* allowDialog= */ false);
        verify(observer).onTabUnregistered(normalTab1);
    }

    @Test
    public void testOnTabUnRegistered_PendingClosure() {
        TabModelSelectorTabRegistrationObserver.Observer observer =
                mock(TabModelSelectorTabRegistrationObserver.Observer.class);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(observer);

        Tab normalTab1 = MockTab.createAndInitialize(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab1,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        verify(observer).onTabRegistered(normalTab1);

        mTabModelSelector
                .getModel(false)
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTab(normalTab1).allowUndo(true).build(),
                        /* allowDialog= */ false);
        mTabModelSelector.getModel(false).commitTabClosure(normalTab1.getId());
        verify(observer).onTabUnregistered(normalTab1);
    }

    @Test
    public void testRemoveObserver() {
        TabModelSelectorTabRegistrationObserver.Observer observer =
                mock(TabModelSelectorTabRegistrationObserver.Observer.class);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(observer);

        Tab normalTab1 = MockTab.createAndInitialize(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab1,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        verify(observer).onTabRegistered(normalTab1);

        mTabRegistrationObserver.removeObserver(observer);

        Tab normalTab2 = MockTab.createAndInitialize(2, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab2,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector
                .getModel(false)
                .getTabRemover()
                .removeTab(normalTab1, /* allowDialog= */ false);

        verifyNoMoreInteractions(observer);
    }

    @Test
    public void testDestroy() {
        TabModelSelectorTabRegistrationObserver.Observer observer =
                mock(TabModelSelectorTabRegistrationObserver.Observer.class);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(observer);

        Tab normalTab1 = MockTab.createAndInitialize(1, mProfile);
        Tab normalTab2 = MockTab.createAndInitialize(2, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab1,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab2,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        verify(observer).onTabRegistered(normalTab1);
        verify(observer).onTabRegistered(normalTab2);

        mTabRegistrationObserver.destroy();
        verify(observer).onTabUnregistered(normalTab1);
        verify(observer).onTabUnregistered(normalTab2);

        Tab normalTab3 = MockTab.createAndInitialize(3, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(
                        normalTab3,
                        0,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelSelector
                .getModel(false)
                .getTabRemover()
                .removeTab(normalTab1, /* allowDialog= */ false);

        verifyNoMoreInteractions(observer);
    }

    private static class TestTabModelSelector extends TabModelSelectorBase {
        public TestTabModelSelector(TabCreatorManager tabCreatorManager) {
            super(tabCreatorManager, false);
        }

        @Override
        public void requestToShowTab(Tab tab, int type) {}

        @Override
        public boolean isTabModelRestored() {
            return true;
        }
    }
}
