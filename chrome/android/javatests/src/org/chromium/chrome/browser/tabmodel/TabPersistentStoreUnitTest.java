// Copyright 2017 The Chromium Authors
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
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.text.TextUtils;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.UserDataHost;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabRestoreDetails;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Unit tests for the tab persistent store logic.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TabPersistentStoreUnitTest {
    private static final Integer RESTORE_TAB_ID_1 = 31;
    private static final Integer RESTORE_TAB_ID_2 = 32;
    private static final Integer RESTORE_TAB_ID_3 = 33;

    private static final String REGULAR_TAB_STRING_1 = "https://foo.com/";
    private static final String INCOGNITO_TAB_STRING_1 = "https://bar.com/";
    private static final String INCOGNITO_TAB_STRING_2 = "https://baz.com/";
    private static final String RESTORE_TAB_STRING_1 = "https://qux.com/";
    private static final String RESTORE_TAB_STRING_2 = "https://quux.com/";
    private static final String RESTORE_TAB_STRING_3 = "https://quuz.com/";

    @Mock
    private TabPersistencePolicy mPersistencePolicy;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mNormalTabModel;
    @Mock
    private TabModel mIncognitoTabModel;
    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;
    private TabModelFilter mNormalTabModelFilter;
    private TabModelFilter mIncognitoTabModelFilter;

    @Mock
    private TabCreatorManager mTabCreatorManager;
    @Mock
    private TabCreator mNormalTabCreator;
    @Mock
    private TabCreator mIncognitoTabCreator;

    private TabPersistentStore mPersistentStore;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        when(mTabCreatorManager.getTabCreator(false)).thenReturn(mNormalTabCreator);
        when(mTabCreatorManager.getTabCreator(true)).thenReturn(mIncognitoTabCreator);

        when(mPersistencePolicy.getStateFileName())
                .thenReturn(TabPersistentStore.SAVED_STATE_FILE_PREFIX + "state_files_yay");
        when(mPersistencePolicy.isMergeInProgress()).thenReturn(false);
        when(mPersistencePolicy.performInitialization(any(TaskRunner.class))).thenReturn(false);

        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        mNormalTabModelFilter = new TabGroupModelFilter(mNormalTabModel);
        mIncognitoTabModelFilter = new TabGroupModelFilter(mIncognitoTabModel);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mNormalTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mIncognitoTabModelFilter);
    }

    @After
    public void tearDown() throws Exception {
        // Flush pending PersistentStore tasks.
        final AtomicBoolean flushed = new AtomicBoolean(false);
        if (mPersistentStore != null) {
            mPersistentStore.getTaskRunnerForTests().postTask(() -> { flushed.set(true); });
            CriteriaHelper.pollUiThread(() -> flushed.get());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature("TabPersistentStore")
    public void testNtpSaveBehavior() {
        when(mNormalTabModel.index()).thenReturn(TabList.INVALID_TAB_INDEX);
        when(mIncognitoTabModel.index()).thenReturn(TabList.INVALID_TAB_INDEX);

        mPersistentStore = new TabPersistentStore(
                mPersistencePolicy, mTabModelSelector, mTabCreatorManager) {
            @Override
            protected void saveNextTab() {
                // Intentionally ignore to avoid triggering async task creation.
            }
        };

        TabImpl emptyNtpTab = mock(TabImpl.class);
        UserDataHost emptyNtpTabUserDataHost = new UserDataHost();
        when(emptyNtpTab.getUserDataHost()).thenReturn(emptyNtpTabUserDataHost);
        TabStateAttributes.createForTab(emptyNtpTab, TabCreationState.FROZEN_ON_RESTORE);
        when(emptyNtpTab.getUrl()).thenReturn(new GURL(UrlConstants.NTP_URL));
        TabStateAttributes.from(emptyNtpTab)
                .setStateForTesting(TabStateAttributes.DirtinessState.DIRTY);
        when(emptyNtpTab.canGoBack()).thenReturn(false);
        when(emptyNtpTab.canGoForward()).thenReturn(false);

        mPersistentStore.addTabToSaveQueue(emptyNtpTab);
        assertFalse(mPersistentStore.isTabPendingSave(emptyNtpTab));

        TabImpl ntpWithBackNavTab = mock(TabImpl.class);
        UserDataHost ntpWithBackNavTabUserDataHost = new UserDataHost();
        when(ntpWithBackNavTab.getUserDataHost()).thenReturn(ntpWithBackNavTabUserDataHost);
        TabStateAttributes.createForTab(ntpWithBackNavTab, TabCreationState.FROZEN_ON_RESTORE);
        when(ntpWithBackNavTab.getUrl()).thenReturn(new GURL(UrlConstants.NTP_URL));
        TabStateAttributes.from(ntpWithBackNavTab)
                .setStateForTesting(TabStateAttributes.DirtinessState.DIRTY);
        when(ntpWithBackNavTab.canGoBack()).thenReturn(true);
        when(ntpWithBackNavTab.canGoForward()).thenReturn(false);

        mPersistentStore.addTabToSaveQueue(ntpWithBackNavTab);
        assertTrue(mPersistentStore.isTabPendingSave(ntpWithBackNavTab));

        TabImpl ntpWithForwardNavTab = mock(TabImpl.class);
        UserDataHost ntpWithForwardNavTabUserDataHost = new UserDataHost();
        when(ntpWithForwardNavTab.getUserDataHost()).thenReturn(ntpWithForwardNavTabUserDataHost);
        TabStateAttributes.createForTab(ntpWithForwardNavTab, TabCreationState.FROZEN_ON_RESTORE);
        when(ntpWithForwardNavTab.getUrl()).thenReturn(new GURL(UrlConstants.NTP_URL));
        TabStateAttributes.from(ntpWithForwardNavTab)
                .setStateForTesting(TabStateAttributes.DirtinessState.DIRTY);
        when(ntpWithForwardNavTab.canGoBack()).thenReturn(false);
        when(ntpWithForwardNavTab.canGoForward()).thenReturn(true);

        mPersistentStore.addTabToSaveQueue(ntpWithForwardNavTab);
        assertTrue(mPersistentStore.isTabPendingSave(ntpWithForwardNavTab));

        TabImpl ntpWithAllTheNavsTab = mock(TabImpl.class);
        UserDataHost ntpWithAllTheNavsTabUserDataHost = new UserDataHost();
        when(ntpWithAllTheNavsTab.getUserDataHost()).thenReturn(ntpWithAllTheNavsTabUserDataHost);
        TabStateAttributes.createForTab(ntpWithAllTheNavsTab, TabCreationState.FROZEN_ON_RESTORE);
        when(ntpWithAllTheNavsTab.getUrl()).thenReturn(new GURL(UrlConstants.NTP_URL));
        TabStateAttributes.from(ntpWithAllTheNavsTab)
                .setStateForTesting(TabStateAttributes.DirtinessState.DIRTY);
        when(ntpWithAllTheNavsTab.canGoBack()).thenReturn(true);
        when(ntpWithAllTheNavsTab.canGoForward()).thenReturn(true);

        mPersistentStore.addTabToSaveQueue(ntpWithAllTheNavsTab);
        assertTrue(mPersistentStore.isTabPendingSave(ntpWithAllTheNavsTab));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testNotActiveEmptyNtpIgnoredDuringRestore() {
        mPersistentStore =
                new TabPersistentStore(mPersistencePolicy, mTabModelSelector, mTabCreatorManager);
        mPersistentStore.initializeRestoreVars(false);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, false);

        verifyNoMoreInteractions(mNormalTabCreator);
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testNotActiveEmptyNtpNotIgnoredDuringRestoreWithSkipNonActiveNtpsFlagEnabled() {
        mPersistentStore =
                new TabPersistentStore(mPersistencePolicy, mTabModelSelector, mTabCreatorManager);
        mPersistentStore.initializeRestoreVars(false);
        mPersistentStore.setSkipSavingNonActiveNtps(true);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, false);

        verify(mNormalTabCreator)
                .createNewTab(argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE), (Tab) isNull(), eq(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testActiveEmptyNtpNotIgnoredDuringRestore() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);

        mPersistentStore =
                new TabPersistentStore(mPersistencePolicy, mTabModelSelector, mTabCreatorManager);
        mPersistentStore.initializeRestoreVars(false);

        LoadUrlParamsUrlMatcher paramsMatcher = new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL);
        TabImpl emptyNtp = mock(TabImpl.class);
        when(mNormalTabCreator.createNewTab(
                     argThat(paramsMatcher), eq(TabLaunchType.FROM_RESTORE), (Tab) isNull()))
                .thenReturn(emptyNtp);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, true);

        verify(mNormalTabCreator)
                .createNewTab(argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE), (Tab) isNull(), eq(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testNtpFromMergeWithNoStateNotIgnoredDuringMerge() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);

        mPersistentStore =
                new TabPersistentStore(mPersistencePolicy, mTabModelSelector, mTabCreatorManager);
        mPersistentStore.initializeRestoreVars(false);

        LoadUrlParamsUrlMatcher paramsMatcher = new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL);
        TabImpl emptyNtp = mock(TabImpl.class);
        when(mNormalTabCreator.createNewTab(
                     argThat(paramsMatcher), eq(TabLaunchType.FROM_RESTORE), (Tab) isNull()))
                .thenReturn(emptyNtp);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, true);
        mPersistentStore.restoreTab(emptyNtpDetails, null, false);
        verify(mNormalTabCreator)
                .createNewTab(argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE), (Tab) isNull(), eq(0));

        TabRestoreDetails emptyIncognitoNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, true);
        mPersistentStore.restoreTab(emptyIncognitoNtpDetails, null, false);
        verify(mIncognitoTabCreator)
                .createNewTab(argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE), (Tab) isNull(), eq(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    // TODO(crbug.com/1119583) Add similar test for CriticalPersistedTabData
    public void testNtpWithStateNotIgnoredDuringRestore() {
        mPersistentStore =
                new TabPersistentStore(mPersistencePolicy, mTabModelSelector, mTabCreatorManager);
        mPersistentStore.initializeRestoreVars(false);

        TabRestoreDetails ntpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, false);
        TabState ntpState = new TabState();
        mPersistentStore.restoreTab(ntpDetails, ntpState, false);

        verify(mNormalTabCreator)
                .createFrozenTab(eq(ntpState), eq(null), eq(1), eq(false), anyInt());
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testActiveEmptyIncognitoNtpNotIgnoredDuringRestore() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);

        mPersistentStore =
                new TabPersistentStore(mPersistencePolicy, mTabModelSelector, mTabCreatorManager);
        mPersistentStore.initializeRestoreVars(false);

        LoadUrlParamsUrlMatcher paramsMatcher = new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL);
        TabImpl emptyNtp = mock(TabImpl.class);
        when(mIncognitoTabCreator.createNewTab(
                     argThat(paramsMatcher), eq(TabLaunchType.FROM_RESTORE), (Tab) isNull()))
                .thenReturn(emptyNtp);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, true);

        verify(mIncognitoTabCreator)
                .createNewTab(argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE), (Tab) isNull(), eq(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testNotActiveIncognitoNtpIgnoredDuringRestore() {
        mPersistentStore =
                new TabPersistentStore(mPersistencePolicy, mTabModelSelector, mTabCreatorManager);
        mPersistentStore.initializeRestoreVars(false);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, false);

        verifyNoMoreInteractions(mIncognitoTabCreator);
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testActiveEmptyIncognitoNtpIgnoredDuringRestoreIfIncognitoLoadingIsDisabled() {
        mPersistentStore =
                new TabPersistentStore(mPersistencePolicy, mTabModelSelector, mTabCreatorManager);
        mPersistentStore.initializeRestoreVars(true);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, true);

        verifyNoMoreInteractions(mIncognitoTabCreator);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature("TabPersistentStore")
    public void testSerializeTabModelSelector() throws IOException {
        setupSerializationTestMocks();
        TabModelSelectorMetadata metadata =
                TabPersistentStore.serializeTabModelSelector(mTabModelSelector, null, false);

        Assert.assertEquals("Incorrect index for regular", 0, metadata.normalModelMetadata.index);
        Assert.assertEquals(
                "Incorrect number of tabs in regular", 2, metadata.normalModelMetadata.ids.size());
        Assert.assertEquals("Incorrect URL for regular tab.", REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(0));
        Assert.assertEquals("Incorrect URL for regular tab.", UrlConstants.NTP_URL,
                metadata.normalModelMetadata.urls.get(1));

        Assert.assertEquals(
                "Incorrect index for incognito", 1, metadata.incognitoModelMetadata.index);
        Assert.assertEquals("Incorrect number of tabs in incognito", 2,
                metadata.incognitoModelMetadata.ids.size());
        Assert.assertEquals("Incorrect URL for first incognito tab.", INCOGNITO_TAB_STRING_1,
                metadata.incognitoModelMetadata.urls.get(0));
        Assert.assertEquals("Incorrect URL for second incognito tab.", INCOGNITO_TAB_STRING_2,
                metadata.incognitoModelMetadata.urls.get(1));

        Assert.assertEquals("Incorrect number of cached normal tab count.", 2,
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.REGULAR_TAB_COUNT));
        Assert.assertEquals("Incorrect number of cached incognito tab count.", 2,
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.INCOGNITO_TAB_COUNT));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature("TabPersistentStore")
    public void testWithoutSkipNonActiveNtps() throws IOException {
        setupSerializationTestMocksWithSkippedNtpComeBeforeActiveTab();
        TabModelSelectorMetadata metadata =
                TabPersistentStore.serializeTabModelSelector(mTabModelSelector, null, false);

        Assert.assertEquals("Incorrect index for regular", 1, metadata.normalModelMetadata.index);
        Assert.assertEquals(
                "Incorrect number of tabs in regular", 2, metadata.normalModelMetadata.ids.size());
        Assert.assertEquals("Incorrect URL for regular tab.", UrlConstants.NTP_URL,
                metadata.normalModelMetadata.urls.get(0));
        Assert.assertEquals("Incorrect URL for regular tab.", REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(1));

        Assert.assertEquals("Incorrect number of cached normal tab count.", 2,
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.REGULAR_TAB_COUNT));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature("TabPersistentStore")
    public void testSkipNonActiveNtpsWithSkippedNtpComeBeforeActiveTab() throws IOException {
        setupSerializationTestMocksWithSkippedNtpComeBeforeActiveTab();
        TabModelSelectorMetadata metadata =
                TabPersistentStore.serializeTabModelSelector(mTabModelSelector, null, true);

        Assert.assertEquals("Incorrect index for regular", 0, metadata.normalModelMetadata.index);
        Assert.assertEquals(
                "Incorrect number of tabs in regular", 1, metadata.normalModelMetadata.ids.size());
        Assert.assertEquals("Incorrect URL for regular tab.", REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(0));

        Assert.assertEquals("Incorrect number of cached normal tab count.", 1,
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.REGULAR_TAB_COUNT));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature("TabPersistentStore")
    public void testSkipNonActiveNtpsWithSkippedNtpComeAfterActiveTab() throws IOException {
        setupSerializationTestMocks();
        TabModelSelectorMetadata metadata =
                TabPersistentStore.serializeTabModelSelector(mTabModelSelector, null, true);

        Assert.assertEquals("Incorrect index for regular", 0, metadata.normalModelMetadata.index);
        Assert.assertEquals(
                "Incorrect number of tabs in regular", 1, metadata.normalModelMetadata.ids.size());
        Assert.assertEquals("Incorrect URL for regular tab.", REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(0));

        Assert.assertEquals("Incorrect number of cached normal tab count.", 1,
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.REGULAR_TAB_COUNT));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature("TabPersistentStore")
    public void testSerializeTabModelSelector_tabsBeingRestored() throws IOException {
        setupSerializationTestMocks();
        TabRestoreDetails regularTabRestoreDetails =
                new TabRestoreDetails(RESTORE_TAB_ID_1, 2, false, RESTORE_TAB_STRING_1, false);
        TabRestoreDetails incognitoTabRestoreDetails =
                new TabRestoreDetails(RESTORE_TAB_ID_2, 3, true, RESTORE_TAB_STRING_2, false);
        TabRestoreDetails unknownTabRestoreDetails =
                new TabRestoreDetails(RESTORE_TAB_ID_3, 4, null, RESTORE_TAB_STRING_3, false);
        ArrayList<TabRestoreDetails> tabRestoreDetails = new ArrayList<TabRestoreDetails>() {};
        tabRestoreDetails.add(regularTabRestoreDetails);
        tabRestoreDetails.add(incognitoTabRestoreDetails);
        tabRestoreDetails.add(unknownTabRestoreDetails);

        TabModelSelectorMetadata metadata = TabPersistentStore.serializeTabModelSelector(
                mTabModelSelector, tabRestoreDetails, true);
        Assert.assertEquals("Incorrect index for regular", 0, metadata.normalModelMetadata.index);
        Assert.assertEquals(
                "Incorrect number of tabs in regular", 2, metadata.normalModelMetadata.ids.size());
        Assert.assertEquals("Incorrect URL for first regular tab.", REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(0));
        Assert.assertEquals("Incorrect URL for first second tab.", RESTORE_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(1));

        // TabRestoreDetails with unknown isIncognito should be appended to incognito list.
        Assert.assertEquals(
                "Incorrect index for incognito", 1, metadata.incognitoModelMetadata.index);
        Assert.assertEquals("Incorrect number of tabs in incognito", 4,
                metadata.incognitoModelMetadata.ids.size());
        Assert.assertEquals("Incorrect URL for first incognito tab.", INCOGNITO_TAB_STRING_1,
                metadata.incognitoModelMetadata.urls.get(0));
        Assert.assertEquals("Incorrect URL for second incognito tab.", INCOGNITO_TAB_STRING_2,
                metadata.incognitoModelMetadata.urls.get(1));
        Assert.assertEquals("Incorrect URL for third incognito tab.", RESTORE_TAB_STRING_2,
                metadata.incognitoModelMetadata.urls.get(2));
        Assert.assertEquals("Incorrect URL for fourth \"incognito\" tab.", RESTORE_TAB_STRING_3,
                metadata.incognitoModelMetadata.urls.get(3));
    }

    private void setupSerializationTestMocks() {
        when(mNormalTabModel.getCount()).thenReturn(2);
        when(mNormalTabModel.index()).thenReturn(0);
        TabImpl regularTab1 = mock(TabImpl.class);
        GURL gurl = new GURL(REGULAR_TAB_STRING_1);
        when(regularTab1.getUrl()).thenReturn(gurl);
        when(mNormalTabModel.getTabAt(0)).thenReturn(regularTab1);

        TabImpl regularNtpTab1 = mock(TabImpl.class);
        GURL ntpGurl = new GURL(UrlConstants.NTP_URL);
        when(regularNtpTab1.getUrl()).thenReturn(ntpGurl);
        when(regularNtpTab1.isNativePage()).thenReturn(true);
        when(mNormalTabModel.getTabAt(1)).thenReturn(regularNtpTab1);

        when(mIncognitoTabModel.getCount()).thenReturn(2);
        when(mIncognitoTabModel.index()).thenReturn(1);
        TabImpl incognitoTab1 = mock(TabImpl.class);
        gurl = new GURL(INCOGNITO_TAB_STRING_1);
        when(incognitoTab1.getUrl()).thenReturn(gurl);
        when(incognitoTab1.isIncognito()).thenReturn(true);
        when(mIncognitoTabModel.getTabAt(0)).thenReturn(incognitoTab1);

        TabImpl incognitoTab2 = mock(TabImpl.class);
        gurl = new GURL(INCOGNITO_TAB_STRING_2);
        when(incognitoTab2.getUrl()).thenReturn(gurl);
        when(incognitoTab2.isIncognito()).thenReturn(true);
        when(mIncognitoTabModel.getTabAt(1)).thenReturn(incognitoTab2);

        when(mTabModelSelector.getTotalTabCount()).thenReturn(4);
    }

    private void setupSerializationTestMocksWithSkippedNtpComeBeforeActiveTab() {
        when(mNormalTabModel.getCount()).thenReturn(2);
        // Sets a non active Ntp is the first Tab, and a regular Tab is the second one and the
        // current active Tab.
        when(mNormalTabModel.index()).thenReturn(1);
        TabImpl regularNtpTab1 = mock(TabImpl.class);
        GURL ntpGurl = new GURL(UrlConstants.NTP_URL);
        when(regularNtpTab1.getUrl()).thenReturn(ntpGurl);
        when(regularNtpTab1.isNativePage()).thenReturn(true);
        when(mNormalTabModel.getTabAt(0)).thenReturn(regularNtpTab1);

        TabImpl regularTab1 = mock(TabImpl.class);
        GURL gurl = new GURL(REGULAR_TAB_STRING_1);
        when(regularTab1.getUrl()).thenReturn(gurl);
        when(mNormalTabModel.getTabAt(1)).thenReturn(regularTab1);

        when(mIncognitoTabModel.getCount()).thenReturn(0);
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
