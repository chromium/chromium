// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;

import java.util.Arrays;

/**
 * Unit tests for {@link TabModelImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelImplUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final long FAKE_NATIVE_ADDRESS = 123L;

    /**
     * Disable native calls from {@link TabModelJniBridge}.
     */
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private TabModelJniBridge.Natives mTabModelJniBridge;
    /**
     * Required to be non-null for {@link TabModelJniBridge}.
     */
    @Mock
    private Profile mProfile;
    /**
     * Required to simulate tab thumbnail deletion.
     */
    @Mock
    private TabContentManager mTabContentManager;
    /**
     * Required to handle some tab lookup actions.
     */
    @Mock
    private TabModelDelegate mTabModelDelegate;
    /**
     * Required to handle some actions and initialize {@link TabModelOrderControllerImpl}.
     */
    @Mock
    private TabModelSelector mTabModelSelector;

    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    private TabModelFilter mTabModelFilter;

    private int mNextTabId;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Disable HomepageManager#shouldCloseAppWithZeroTabs() for TabModelImpl#closeAllTabs().
        HomepageManager.getInstance().setPrefHomepageEnabled(false);

        mJniMocker.mock(TabModelJniBridgeJni.TEST_HOOKS, mTabModelJniBridge);
        when(mTabModelJniBridge.init(any(), any(), anyInt())).thenReturn(FAKE_NATIVE_ADDRESS);

        when(mTabModelDelegate.isReparentingInProgress()).thenReturn(false);

        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mTabModelFilter);
        when(mTabModelFilter.getValidPosition(any(), anyInt()))
                .thenAnswer(i -> i.getArguments()[1]);

        mNextTabId = 0;
    }

    private Tab createTab(final TabModel model) {
        return createTab(model, 0, Tab.INVALID_TAB_ID);
    }

    private Tab createTab(final TabModel model, long activeTimestampMillis, int parentId) {
        final int launchType = TabLaunchType.FROM_CHROME_UI;
        MockTab tab = new MockTab(mNextTabId++, model.isIncognito());
        CriticalPersistedTabData data = new CriticalPersistedTabData(tab);
        data.setTimestampMillis(activeTimestampMillis);
        data.setParentId(parentId);
        tab = (MockTab) MockTab.initializeWithCriticalPersistedTabData(tab, data);
        tab.setIsInitialized(true);
        model.addTab(tab, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        return tab;
    }

    private void selectTab(final TabModel model, final Tab tab) {
        model.setIndex(model.indexOf(tab), TabSelectionType.FROM_USER, false);
    }

    /**
     * Create a {@link TabModel} to use for the test.
     */
    private TabModel createTabModel(boolean isActive, boolean isIncognito) {
        AsyncTabParamsManager realAsyncTabParamsManager =
                AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        TabModelOrderControllerImpl orderController =
                new TabModelOrderControllerImpl(mTabModelSelector);
        TabModel tabModel;
        when(mProfile.isOffTheRecord()).thenReturn(isIncognito);
        tabModel = new TabModelImpl(mProfile, ActivityType.TABBED,
                /*regularTabCreator=*/null, /*incognitoTabCreator=*/null, orderController,
                mTabContentManager,
                ()
                        -> NextTabPolicy.HIERARCHICAL,
                realAsyncTabParamsManager, mTabModelDelegate, /*supportsUndo=*/false);
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

        Assert.assertEquals(incognitoTab1, inactiveNormal.getNextTabIfClosed(tab1.getId(), false));
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
        Assert.assertEquals(tab0, activeNormal.getNextTabIfClosed(tab1.getId(), false));
        Assert.assertEquals(tab0, activeNormal.getNextTabIfClosed(tab2.getId(), false));

        selectTab(activeNormal, tab1);
        Assert.assertEquals(tab1, activeNormal.getNextTabIfClosed(tab0.getId(), false));
        Assert.assertEquals(tab1, activeNormal.getNextTabIfClosed(tab2.getId(), false));

        selectTab(activeNormal, tab2);
        Assert.assertEquals(tab2, activeNormal.getNextTabIfClosed(tab0.getId(), false));
        Assert.assertEquals(tab2, activeNormal.getNextTabIfClosed(tab1.getId(), false));
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
        Assert.assertEquals(tab0, activeNormal.getNextTabIfClosed(tab2.getId(), false));
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
        Assert.assertEquals(tab1, activeNormal.getNextTabIfClosed(tab0.getId(), false));

        selectTab(activeNormal, tab1);
        Assert.assertEquals(tab0, activeNormal.getNextTabIfClosed(tab1.getId(), false));

        selectTab(activeNormal, tab2);
        Assert.assertEquals(tab1, activeNormal.getNextTabIfClosed(tab2.getId(), false));
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
        Assert.assertEquals(tab0, activeIncognito.getNextTabIfClosed(incognitoTab0.getId(), false));

        selectTab(inactiveNormal, tab1);
        Assert.assertEquals(tab1, activeIncognito.getNextTabIfClosed(incognitoTab0.getId(), false));
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
        Assert.assertEquals(tab1, activeNormal.getNextTabIfClosed(tab0.getId(), true));

        selectTab(activeNormal, tab1);
        Assert.assertEquals(tab2, activeNormal.getNextTabIfClosed(tab1.getId(), true));

        selectTab(activeNormal, tab2);
        Assert.assertEquals(tab1, activeNormal.getNextTabIfClosed(tab2.getId(), true));
    }

    @Test
    @SmallTest
    public void testGetNextTabIfClosed_InvalidSelection() {
        TabModel activeNormal = createTabModel(true, false);
        // Unused but required for correct mocking of mTabModelDelegate to avoid NPE.
        TabModel inactiveIncognito = createTabModel(false, true);

        Tab tab0 = createTab(activeNormal);
        selectTab(activeNormal, tab0);
        Assert.assertNull(activeNormal.getNextTabIfClosed(tab0.getId(), false));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_STATE_V1_OPTIMIZATIONS)
    public void testDontSwitchModelsIfIncognitoGroupClosed() {
        TabModel activeIncognito = createTabModel(true, true);
        TabModel inactiveNormal = createTabModel(false, false);

        Tab incognitoTab0 = createTab(activeIncognito);
        Tab incognitoTab1 = createTab(activeIncognito);
        Tab incognitoTab2 = createTab(activeIncognito);
        Tab tab0 = createTab(inactiveNormal);

        selectTab(activeIncognito, incognitoTab0);

        activeIncognito.closeMultipleTabs(
                Arrays.asList(new Tab[] {incognitoTab0, incognitoTab1}), false);
        verify(mTabModelSelector, never()).selectModel(anyBoolean());
        Assert.assertEquals(incognitoTab2, activeIncognito.getTabAt(activeIncognito.index()));
    }
}
