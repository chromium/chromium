// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.eq;
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
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;

/** Unit test for {@link CloseAllTabsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CloseAllTabsHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mRegularTabGroupModelFilter;
    @Mock private TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Mock private TabModel mRegularTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private TabModel mArchivedTabModel;
    @Mock private TabRemover mRegularTabRemover;
    @Mock private TabRemover mIncognitoTabRemover;
    @Mock private Profile mProfile;
    @Mock private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    @Mock private TabArchiver mTabArchiver;

    @Before
    public void setUp() {
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .thenReturn(mRegularTabGroupModelFilter);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(true))
                .thenReturn(mIncognitoTabGroupModelFilter);
        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mRegularTabModel.getTabRemover()).thenReturn(mRegularTabRemover);
        when(mIncognitoTabModel.getTabRemover()).thenReturn(mIncognitoTabRemover);

        // Setup deps for tab archiving.
        when(mTabModelSelector.getCurrentModel()).thenReturn(mRegularTabModel);
        when(mRegularTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mArchivedTabModelOrchestrator.getTabArchiver()).thenReturn(mTabArchiver);
        when(mArchivedTabModelOrchestrator.getTabModel()).thenReturn(mArchivedTabModel);
        ArchivedTabModelOrchestrator.setInstanceForTesting(mArchivedTabModelOrchestrator);
    }

    @Test
    public void testCloseAllTabsHidingTabGroups() {
        CloseAllTabsHelper.closeAllTabsHidingTabGroups(mTabModelSelector, mRegularTabCreator);

        verify(mRegularTabRemover).closeTabs(argThat(params -> params.isAllTabs), eq(false));
        verify(mIncognitoTabRemover).closeTabs(argThat(params -> params.isAllTabs), eq(false));
    }

    @Test
    public void testBuildCloseAllTabsRunnable_Regular() {
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mTabModelSelector, mRegularTabCreator, /* isIncognitoOnly= */ false);
        r.run();

        verify(mRegularTabRemover).closeTabs(argThat(params -> params.isAllTabs), eq(false));
        verify(mIncognitoTabRemover).closeTabs(argThat(params -> params.isAllTabs), eq(false));
    }

    @Test
    public void testBuildCloseAllTabsRunnable_Incognito() {
        Runnable r =
                CloseAllTabsHelper.buildCloseAllTabsRunnable(
                        mTabModelSelector, mRegularTabCreator, /* isIncognitoOnly= */ true);
        r.run();

        verify(mIncognitoTabRemover).closeTabs(argThat(params -> params.isAllTabs), eq(false));

        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
    }
}
