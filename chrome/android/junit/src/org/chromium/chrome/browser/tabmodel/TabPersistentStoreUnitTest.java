// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import android.text.TextUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.task.TaskRunner;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabRestoreDetails;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Unit tests for the tab persistent store logic.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabPersistentStoreUnitTest {
    @Mock
    private TabPersistencePolicy mPersistencePolicy;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mNormalTabModel;
    @Mock
    private TabModel mIncognitoTabModel;

    @Mock
    private TabCreatorManager mTabCreatorManager;
    @Mock
    private TabCreator mNormalTabCreator;
    @Mock
    private TabCreator mIncognitoTabCreator;

    @Mock
    private TabPersistentStoreObserver mObserver;

    private TabPersistentStore mPersistentStore;

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);

        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        when(mTabCreatorManager.getTabCreator(false)).thenReturn(mNormalTabCreator);
        when(mTabCreatorManager.getTabCreator(true)).thenReturn(mIncognitoTabCreator);

        when(mPersistencePolicy.getStateFileName())
                .thenReturn(TabPersistencePolicy.SAVED_STATE_FILE_PREFIX + "state_files_yay");
        when(mPersistencePolicy.isMergeInProgress()).thenReturn(false);
        when(mPersistencePolicy.performInitialization(any(TaskRunner.class))).thenReturn(false);
    }

    @After
    public void afterTest() {
        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
    }

    @Test
    @Feature("TabPersistentStore")
    public void testNtpSaveBehavior() {
        when(mNormalTabModel.index()).thenReturn(TabList.INVALID_TAB_INDEX);
        when(mIncognitoTabModel.index()).thenReturn(TabList.INVALID_TAB_INDEX);

        mPersistentStore = new TabPersistentStore(
                mPersistencePolicy, mTabModelSelector, mTabCreatorManager, mObserver) {
            @Override
            protected void saveNextTab() {
                // Intentionally ignore to avoid triggering async task creation.
            }
        };

        Tab emptyNtpTab = mock(Tab.class);
        when(emptyNtpTab.getUrl()).thenReturn(UrlConstants.NTP_URL);
        when(emptyNtpTab.isTabStateDirty()).thenReturn(true);
        when(emptyNtpTab.canGoBack()).thenReturn(false);
        when(emptyNtpTab.canGoForward()).thenReturn(false);

        mPersistentStore.addTabToSaveQueue(emptyNtpTab);
        assertFalse(mPersistentStore.isTabPendingSave(emptyNtpTab));

        Tab ntpWithBackNavTab = mock(Tab.class);
        when(ntpWithBackNavTab.getUrl()).thenReturn(UrlConstants.NTP_URL);
        when(ntpWithBackNavTab.isTabStateDirty()).thenReturn(true);
        when(ntpWithBackNavTab.canGoBack()).thenReturn(true);
        when(ntpWithBackNavTab.canGoForward()).thenReturn(false);

        mPersistentStore.addTabToSaveQueue(ntpWithBackNavTab);
        assertTrue(mPersistentStore.isTabPendingSave(ntpWithBackNavTab));

        Tab ntpWithForwardNavTab = mock(Tab.class);
        when(ntpWithForwardNavTab.getUrl()).thenReturn(UrlConstants.NTP_URL);
        when(ntpWithForwardNavTab.isTabStateDirty()).thenReturn(true);
        when(ntpWithForwardNavTab.canGoBack()).thenReturn(false);
        when(ntpWithForwardNavTab.canGoForward()).thenReturn(true);

        mPersistentStore.addTabToSaveQueue(ntpWithForwardNavTab);
        assertTrue(mPersistentStore.isTabPendingSave(ntpWithForwardNavTab));

        Tab ntpWithAllTheNavsTab = mock(Tab.class);
        when(ntpWithAllTheNavsTab.getUrl()).thenReturn(UrlConstants.NTP_URL);
        when(ntpWithAllTheNavsTab.isTabStateDirty()).thenReturn(true);
        when(ntpWithAllTheNavsTab.canGoBack()).thenReturn(true);
        when(ntpWithAllTheNavsTab.canGoForward()).thenReturn(true);

        mPersistentStore.addTabToSaveQueue(ntpWithAllTheNavsTab);
        assertTrue(mPersistentStore.isTabPendingSave(ntpWithAllTheNavsTab));
    }

    @Test
    @Feature("TabPersistentStore")
    public void testNotActiveEmptyNtpIgnoredDuringRestore() {
        mPersistentStore = new TabPersistentStore(
                mPersistencePolicy, mTabModelSelector, mTabCreatorManager, mObserver);
        mPersistentStore.initializeRestoreVars(false);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, false);

        verifyZeroInteractions(mNormalTabCreator);
    }

    @Test
    @Feature("TabPersistentStore")
    public void testActiveEmptyNtpNotIgnoredDuringRestore() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);

        mPersistentStore = new TabPersistentStore(
                mPersistencePolicy, mTabModelSelector, mTabCreatorManager, mObserver);
        mPersistentStore.initializeRestoreVars(false);

        LoadUrlParamsUrlMatcher paramsMatcher = new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL);
        Tab emptyNtp = mock(Tab.class);
        when(mNormalTabCreator.createNewTab(
                     argThat(paramsMatcher), eq(TabLaunchType.FROM_RESTORE), (Tab) isNull()))
                .thenReturn(emptyNtp);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, true);

        verify(mNormalTabCreator)
                .createNewTab(argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE), (Tab) isNull());
    }

    @Test
    @Feature("TabPersistentStore")
    public void testNtpFromMergeWithNoStateNotIgnoredDuringMerge() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);

        mPersistentStore = new TabPersistentStore(
                mPersistencePolicy, mTabModelSelector, mTabCreatorManager, mObserver);
        mPersistentStore.initializeRestoreVars(false);

        LoadUrlParamsUrlMatcher paramsMatcher = new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL);
        Tab emptyNtp = mock(Tab.class);
        when(mNormalTabCreator.createNewTab(
                     argThat(paramsMatcher), eq(TabLaunchType.FROM_RESTORE), (Tab) isNull()))
                .thenReturn(emptyNtp);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, true);
        mPersistentStore.restoreTab(emptyNtpDetails, null, false);
        verify(mNormalTabCreator)
                .createNewTab(argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE), (Tab) isNull());

        TabRestoreDetails emptyIncognitoNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, true);
        mPersistentStore.restoreTab(emptyIncognitoNtpDetails, null, false);
        verify(mIncognitoTabCreator)
                .createNewTab(argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE), (Tab) isNull());
    }

    @Test
    @Feature("TabPersistentStore")
    public void testNtpWithStateNotIgnoredDuringRestore() {
        mPersistentStore = new TabPersistentStore(
                mPersistencePolicy, mTabModelSelector, mTabCreatorManager, mObserver);
        mPersistentStore.initializeRestoreVars(false);

        TabRestoreDetails ntpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, false);
        TabState ntpState = new TabState();
        mPersistentStore.restoreTab(ntpDetails, ntpState, false);

        verify(mNormalTabCreator).createFrozenTab(eq(ntpState), eq(1), anyInt());
    }

    @Test
    @Feature("TabPersistentStore")
    public void testActiveEmptyIncognitoNtpNotIgnoredDuringRestore() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);

        mPersistentStore = new TabPersistentStore(
                mPersistencePolicy, mTabModelSelector, mTabCreatorManager, mObserver);
        mPersistentStore.initializeRestoreVars(false);

        LoadUrlParamsUrlMatcher paramsMatcher = new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL);
        Tab emptyNtp = mock(Tab.class);
        when(mIncognitoTabCreator.createNewTab(
                     argThat(paramsMatcher), eq(TabLaunchType.FROM_RESTORE), (Tab) isNull()))
                .thenReturn(emptyNtp);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, true);

        verify(mIncognitoTabCreator)
                .createNewTab(argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE), (Tab) isNull());
    }

    @Test
    @Feature("TabPersistentStore")
    public void testNotActiveIncognitoNtpIgnoredDuringRestore() {
        mPersistentStore = new TabPersistentStore(
                mPersistencePolicy, mTabModelSelector, mTabCreatorManager, mObserver);
        mPersistentStore.initializeRestoreVars(false);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, false);

        verifyZeroInteractions(mIncognitoTabCreator);
    }

    @Test
    @Feature("TabPersistentStore")
    public void testActiveEmptyIncognitoNtpIgnoredDuringRestoreIfIncognitoLoadingIsDisabled() {
        mPersistentStore = new TabPersistentStore(
                mPersistencePolicy, mTabModelSelector, mTabCreatorManager, mObserver);
        mPersistentStore.initializeRestoreVars(true);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, true);

        verifyZeroInteractions(mIncognitoTabCreator);
    }

    private static class LoadUrlParamsUrlMatcher implements ArgumentMatcher<LoadUrlParams> {
        private final String mUrl;

        LoadUrlParamsUrlMatcher(String url) {
            mUrl = url;
        }

        @Override
        public boolean matches(LoadUrlParams argument) {
            return TextUtils.equals(mUrl, argument.getUrl());
        }
    }
}
