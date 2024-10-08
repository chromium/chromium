// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

/** Unit test for {@link CloseAllTabsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.GTS_CLOSE_TAB_ANIMATION_KILL_SWITCH)
public class CloseAllTabsHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabGroupModelFilter mRegularTabGroupModelFilter;
    @Mock private TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Mock private TabModel mRegularTabModel;
    @Mock private TabModel mIncognitoTabModel;

    @Before
    public void setUp() {
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false))
                .thenReturn(mRegularTabGroupModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true))
                .thenReturn(mIncognitoTabGroupModelFilter);
        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
    }

    @Test
    public void testCloseAllTabsHidingTabGroups() {
        CloseAllTabsHelper.closeAllTabsHidingTabGroups(mTabModelSelector, mRegularTabCreator);

        verify(mRegularTabGroupModelFilter).closeTabs(argThat(params -> params.isAllTabs));
        verify(mIncognitoTabGroupModelFilter).closeTabs(argThat(params -> params.isAllTabs));
    }

    @Test
    public void testBuildCloseAllTabsRunnable_Regular() {
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mTabModelSelector, mRegularTabCreator, /* isIncognitoOnly= */ false);
        r.run();

        verify(mRegularTabGroupModelFilter).closeTabs(argThat(params -> params.isAllTabs));
        verify(mIncognitoTabGroupModelFilter).closeTabs(argThat(params -> params.isAllTabs));
    }

    @Test
    public void testBuildCloseAllTabsRunnable_Incognito() {
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mTabModelSelector, mRegularTabCreator, /* isIncognitoOnly= */ true);
        r.run();

        verify(mIncognitoTabModel).closeTabs(argThat(params -> params.isAllTabs));

        verify(mRegularTabGroupModelFilter, never()).closeTabs(any());
        verify(mIncognitoTabGroupModelFilter, never()).closeTabs(any());
    }
}
