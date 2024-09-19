// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.text.TextUtils;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.UserDataHost;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabRestoreDetails;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for the tab persistent store logic. */
@RunWith(BaseRobolectricTestRunner.class)
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabPersistencePolicy mPersistencePolicy;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mNormalTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabCreator mNormalTabCreator;
    @Mock private TabCreator mIncognitoTabCreator;
    @Mock private TabWindowManager mTabWindowManager;

    private TabModelFilter mNormalTabModelFilter;
    private TabModelFilter mIncognitoTabModelFilter;
    private TabPersistentStore mPersistentStore;
    private CipherFactory mCipherFactory;

    @Before
    public void setUp() {
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        when(mTabCreatorManager.getTabCreator(false)).thenReturn(mNormalTabCreator);
        when(mTabCreatorManager.getTabCreator(true)).thenReturn(mIncognitoTabCreator);

        when(mPersistencePolicy.getMetadataFileName())
                .thenReturn(TabPersistentStore.SAVED_METADATA_FILE_PREFIX + "state_files_yay");
        when(mPersistencePolicy.isMergeInProgress()).thenReturn(false);
        when(mPersistencePolicy.performInitialization(any(TaskRunner.class))).thenReturn(false);

        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        mNormalTabModelFilter = new TabGroupModelFilter(mNormalTabModel);
        mIncognitoTabModelFilter = new TabGroupModelFilter(mIncognitoTabModel);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mNormalTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mIncognitoTabModelFilter);

        mCipherFactory = new CipherFactory();
    }

    @After
    public void tearDown() throws Exception {
        // Flush pending PersistentStore tasks.
        final AtomicBoolean flushed = new AtomicBoolean(false);
        if (mPersistentStore != null) {
            mPersistentStore.getTaskRunnerForTests().execute(() -> flushed.set(true));
            CriteriaHelper.pollUiThreadForJUnit(() -> flushed.get());
        }
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testNtpSaveBehavior() {
        when(mNormalTabModel.index()).thenReturn(TabList.INVALID_TAB_INDEX);
        when(mIncognitoTabModel.index()).thenReturn(TabList.INVALID_TAB_INDEX);

        mPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory) {
                    @Override
                    protected void saveNextTab() {
                        // Intentionally ignore to avoid triggering async task creation.
                    }
                };

        Tab emptyNtpTab = mock(Tab.class);
        UserDataHost emptyNtpTabUserDataHost = new UserDataHost();
        when(emptyNtpTab.getUserDataHost()).thenReturn(emptyNtpTabUserDataHost);
        TabStateAttributes.createForTab(emptyNtpTab, TabCreationState.FROZEN_ON_RESTORE);
        when(emptyNtpTab.getUrl()).thenReturn(new GURL(UrlConstants.NTP_URL));
        TabStateAttributes.from(emptyNtpTab)
                .setStateForTesting(TabStateAttributes.DirtinessState.DIRTY);

        mPersistentStore.addTabToSaveQueue(emptyNtpTab);
        assertTrue(mPersistentStore.isTabPendingSave(emptyNtpTab));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testNotActiveEmptyNtpNotIgnoredDuringRestore() {
        mPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.initializeRestoreVars(false);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, false);

        verify(mNormalTabCreator)
                .createNewTab(
                        argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE),
                        isNull(),
                        eq(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testActiveEmptyNtpNotIgnoredDuringRestore() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);

        mPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.initializeRestoreVars(false);

        LoadUrlParamsUrlMatcher paramsMatcher = new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL);
        Tab emptyNtp = mock(Tab.class);
        when(mNormalTabCreator.createNewTab(
                        argThat(paramsMatcher), eq(TabLaunchType.FROM_RESTORE), isNull()))
                .thenReturn(emptyNtp);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, true);

        verify(mNormalTabCreator)
                .createNewTab(
                        argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE),
                        isNull(),
                        eq(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testNtpFromMergeWithNoStateNotIgnoredDuringMerge() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);

        mPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.initializeRestoreVars(false);

        LoadUrlParamsUrlMatcher paramsMatcher = new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL);
        Tab emptyNtp = mock(Tab.class);
        when(mNormalTabCreator.createNewTab(
                        argThat(paramsMatcher), eq(TabLaunchType.FROM_RESTORE), isNull()))
                .thenReturn(emptyNtp);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, true);
        mPersistentStore.restoreTab(emptyNtpDetails, null, false);
        verify(mNormalTabCreator)
                .createNewTab(
                        argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE),
                        isNull(),
                        eq(0));

        TabRestoreDetails emptyIncognitoNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, true);
        mPersistentStore.restoreTab(emptyIncognitoNtpDetails, null, false);
        verify(mIncognitoTabCreator)
                .createNewTab(
                        argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE),
                        isNull(),
                        eq(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testNtpWithStateNotIgnoredDuringRestore() {
        mPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.initializeRestoreVars(false);

        TabRestoreDetails ntpDetails =
                new TabRestoreDetails(1, 0, false, UrlConstants.NTP_URL, false);
        TabState ntpState = new TabState();
        mPersistentStore.restoreTab(ntpDetails, ntpState, false);

        verify(mNormalTabCreator).createFrozenTab(eq(ntpState), eq(1), anyInt());
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testActiveEmptyIncognitoNtpNotIgnoredDuringRestore() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);

        mPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.initializeRestoreVars(false);

        LoadUrlParamsUrlMatcher paramsMatcher = new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL);
        Tab emptyNtp = mock(Tab.class);
        when(mIncognitoTabCreator.createNewTab(
                        argThat(paramsMatcher), eq(TabLaunchType.FROM_RESTORE), isNull()))
                .thenReturn(emptyNtp);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, true);

        verify(mIncognitoTabCreator)
                .createNewTab(
                        argThat(new LoadUrlParamsUrlMatcher(UrlConstants.NTP_URL)),
                        eq(TabLaunchType.FROM_RESTORE),
                        isNull(),
                        eq(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testNotActiveIncognitoNtpIgnoredDuringRestore() {
        mPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
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
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.initializeRestoreVars(true);

        TabRestoreDetails emptyNtpDetails =
                new TabRestoreDetails(1, 0, true, UrlConstants.NTP_URL, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, true);

        verifyNoMoreInteractions(mIncognitoTabCreator);
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_DEDUPE_TAB_IDS_KILL_SWITCH)
    public void testDuplicateTabIds() {
        mPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.initializeRestoreVars(false);

        TabRestoreDetails regularTabRestoreDetails =
                new TabRestoreDetails(RESTORE_TAB_ID_1, 2, false, RESTORE_TAB_STRING_1, false);
        TabRestoreDetails regularTabRestoreDetailsDupe =
                new TabRestoreDetails(RESTORE_TAB_ID_1, 2, false, RESTORE_TAB_STRING_1, false);
        TabState state = new TabState();
        mPersistentStore.restoreTab(regularTabRestoreDetails, state, false);
        mPersistentStore.restoreTab(regularTabRestoreDetailsDupe, state, false);

        // Restoring a dupe should only create a single tab, and skip the second.
        verify(mNormalTabCreator, times(1)).createFrozenTab(eq(state), eq(RESTORE_TAB_ID_1), eq(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testSerializeTabModelSelector() throws IOException {
        setupSerializationTestMocks();
        TabModelSelectorMetadata metadata =
                TabPersistentStore.saveTabModelSelectorMetadata(mTabModelSelector, null);

        assertEquals("Incorrect index for regular", 0, metadata.normalModelMetadata.index);
        // Verifies that the non-active NTP isn't saved.
        assertEquals(
                "Incorrect number of tabs in regular", 1, metadata.normalModelMetadata.ids.size());
        assertEquals(
                "Incorrect URL for regular tab.",
                REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(0));

        assertEquals("Incorrect index for incognito", 1, metadata.incognitoModelMetadata.index);
        assertEquals(
                "Incorrect number of tabs in incognito",
                2,
                metadata.incognitoModelMetadata.ids.size());
        assertEquals(
                "Incorrect URL for first incognito tab.",
                INCOGNITO_TAB_STRING_1,
                metadata.incognitoModelMetadata.urls.get(0));
        assertEquals(
                "Incorrect URL for second incognito tab.",
                INCOGNITO_TAB_STRING_2,
                metadata.incognitoModelMetadata.urls.get(1));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testSkipNonActiveNtpsWithSkippedNtpComeBeforeActiveTab() throws IOException {
        setupSerializationTestMocksWithSkippedNtpComeBeforeActiveTab();
        TabModelSelectorMetadata metadata =
                TabPersistentStore.saveTabModelSelectorMetadata(mTabModelSelector, null);

        assertEquals("Incorrect index for regular", 0, metadata.normalModelMetadata.index);
        assertEquals(
                "Incorrect number of tabs in regular", 1, metadata.normalModelMetadata.ids.size());
        assertEquals(
                "Incorrect URL for regular tab.",
                REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testSkipNonActiveNtpsWithSkippedNtpComeAfterActiveTab() throws IOException {
        setupSerializationTestMocks();
        TabModelSelectorMetadata metadata =
                TabPersistentStore.saveTabModelSelectorMetadata(mTabModelSelector, null);

        assertEquals("Incorrect index for regular", 0, metadata.normalModelMetadata.index);
        assertEquals(
                "Incorrect number of tabs in regular", 1, metadata.normalModelMetadata.ids.size());
        assertEquals(
                "Incorrect URL for regular tab.",
                REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(0));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testSkipNonActiveNtpsWithGroupedAndNavigableNtps_TabGroupStableIdsEnabled()
            throws IOException {
        setupSerializationTestMocksWithGroupedAndNavigableNtps();
        TabModelSelectorMetadata metadata =
                TabPersistentStore.saveTabModelSelectorMetadata(mTabModelSelector, null);

        assertEquals("Incorrect index for regular", 1, metadata.normalModelMetadata.index);
        assertEquals(
                "Incorrect number of tabs in regular", 2, metadata.normalModelMetadata.ids.size());
        assertEquals(
                "Incorrect URL for first NTP.",
                UrlConstants.NTP_URL,
                metadata.normalModelMetadata.urls.get(0));
        assertEquals(
                "Incorrect id for first NTP.",
                1,
                metadata.normalModelMetadata.ids.get(0).intValue());
        assertEquals(
                "Incorrect URL for regular tab.",
                REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(1));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testSerializeTabModelSelector_tabsBeingRestored() throws IOException {
        setupSerializationTestMocks();
        TabRestoreDetails regularTabRestoreDetails =
                new TabRestoreDetails(RESTORE_TAB_ID_1, 2, false, RESTORE_TAB_STRING_1, false);
        TabRestoreDetails incognitoTabRestoreDetails =
                new TabRestoreDetails(RESTORE_TAB_ID_2, 3, true, RESTORE_TAB_STRING_2, false);
        TabRestoreDetails unknownTabRestoreDetails =
                new TabRestoreDetails(RESTORE_TAB_ID_3, 4, null, RESTORE_TAB_STRING_3, false);
        List<TabRestoreDetails> tabRestoreDetails = new ArrayList<>();
        tabRestoreDetails.add(regularTabRestoreDetails);
        tabRestoreDetails.add(incognitoTabRestoreDetails);
        tabRestoreDetails.add(unknownTabRestoreDetails);

        TabModelSelectorMetadata metadata =
                TabPersistentStore.saveTabModelSelectorMetadata(
                        mTabModelSelector, tabRestoreDetails);
        assertEquals("Incorrect index for regular", 0, metadata.normalModelMetadata.index);
        assertEquals(
                "Incorrect number of tabs in regular", 2, metadata.normalModelMetadata.ids.size());
        assertEquals(
                "Incorrect URL for first regular tab.",
                REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(0));
        assertEquals(
                "Incorrect URL for first second tab.",
                RESTORE_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(1));

        // TabRestoreDetails with unknown isIncognito should be appended to incognito list.
        assertEquals("Incorrect index for incognito", 1, metadata.incognitoModelMetadata.index);
        assertEquals(
                "Incorrect number of tabs in incognito",
                4,
                metadata.incognitoModelMetadata.ids.size());
        assertEquals(
                "Incorrect URL for first incognito tab.",
                INCOGNITO_TAB_STRING_1,
                metadata.incognitoModelMetadata.urls.get(0));
        assertEquals(
                "Incorrect URL for second incognito tab.",
                INCOGNITO_TAB_STRING_2,
                metadata.incognitoModelMetadata.urls.get(1));
        assertEquals(
                "Incorrect URL for third incognito tab.",
                RESTORE_TAB_STRING_2,
                metadata.incognitoModelMetadata.urls.get(2));
        assertEquals(
                "Incorrect URL for fourth \"incognito\" tab.",
                RESTORE_TAB_STRING_3,
                metadata.incognitoModelMetadata.urls.get(3));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testSerializeTabModelSelector_closingTabsSkipped() throws IOException {
        when(mNormalTabModel.getCount()).thenReturn(2);
        when(mNormalTabModel.index()).thenReturn(1);
        Tab regularTab1 = mock(Tab.class);
        when(regularTab1.getId()).thenReturn(11);
        when(regularTab1.getUrl()).thenReturn(new GURL(REGULAR_TAB_STRING_1));
        when(regularTab1.isClosing()).thenReturn(false);
        when(mNormalTabModel.getTabAt(0)).thenReturn(regularTab1);
        Tab regularTab2 = mock(Tab.class);
        when(regularTab2.getId()).thenReturn(22);
        when(regularTab2.getUrl()).thenReturn(new GURL(RESTORE_TAB_STRING_2));
        when(regularTab2.isClosing()).thenReturn(true);
        when(mNormalTabModel.getTabAt(1)).thenReturn(regularTab2);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);

        TabModelSelectorMetadata metadata =
                TabPersistentStore.saveTabModelSelectorMetadata(mTabModelSelector, null);

        assertEquals(1, metadata.normalModelMetadata.ids.size());
        assertEquals(1, metadata.normalModelMetadata.urls.size());
        assertEquals(0, metadata.normalModelMetadata.index);
        assertEquals(11, metadata.normalModelMetadata.ids.get(0).intValue());
        assertEquals(REGULAR_TAB_STRING_1, metadata.normalModelMetadata.urls.get(0));
    }

    private void setupSerializationTestMocks() {
        when(mNormalTabModel.getCount()).thenReturn(2);
        when(mNormalTabModel.index()).thenReturn(0);
        Tab regularTab1 = mock(Tab.class);
        GURL gurl = new GURL(REGULAR_TAB_STRING_1);
        when(regularTab1.getUrl()).thenReturn(gurl);
        when(mNormalTabModel.getTabAt(0)).thenReturn(regularTab1);

        Tab regularNtpTab1 = mock(Tab.class);
        GURL ntpGurl = new GURL(UrlConstants.NTP_URL);
        when(regularNtpTab1.getUrl()).thenReturn(ntpGurl);
        when(regularNtpTab1.isNativePage()).thenReturn(true);
        when(mNormalTabModel.getTabAt(1)).thenReturn(regularNtpTab1);

        when(mIncognitoTabModel.getCount()).thenReturn(2);
        when(mIncognitoTabModel.index()).thenReturn(1);
        Tab incognitoTab1 = mock(Tab.class);
        gurl = new GURL(INCOGNITO_TAB_STRING_1);
        when(incognitoTab1.getUrl()).thenReturn(gurl);
        when(incognitoTab1.isIncognito()).thenReturn(true);
        when(mIncognitoTabModel.getTabAt(0)).thenReturn(incognitoTab1);

        Tab incognitoTab2 = mock(Tab.class);
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
        Tab regularNtpTab1 = mock(Tab.class);
        GURL ntpGurl = new GURL(UrlConstants.NTP_URL);
        when(regularNtpTab1.getUrl()).thenReturn(ntpGurl);
        when(regularNtpTab1.isNativePage()).thenReturn(true);
        when(mNormalTabModel.getTabAt(0)).thenReturn(regularNtpTab1);

        Tab regularTab1 = mock(Tab.class);
        GURL gurl = new GURL(REGULAR_TAB_STRING_1);
        when(regularTab1.getUrl()).thenReturn(gurl);
        when(mNormalTabModel.getTabAt(1)).thenReturn(regularTab1);

        when(mIncognitoTabModel.getCount()).thenReturn(0);
    }

    private void setupSerializationTestMocksWithGroupedAndNavigableNtps() {
        when(mNormalTabModel.getCount()).thenReturn(3);
        when(mNormalTabModel.index()).thenReturn(2);

        GURL ntpGurl = new GURL(UrlConstants.NTP_URL);

        // Non-active NTP with no state.
        Tab regularNtpTab1 = mock(Tab.class);
        when(regularNtpTab1.getId()).thenReturn(0);
        when(regularNtpTab1.getUrl()).thenReturn(ntpGurl);
        when(regularNtpTab1.isNativePage()).thenReturn(true);
        when(mNormalTabModel.getTabAt(0)).thenReturn(regularNtpTab1);

        // Non-active NTP with tab group.
        Tab regularNtpTab2 = mock(Tab.class);
        when(regularNtpTab2.getId()).thenReturn(1);
        when(regularNtpTab2.getUrl()).thenReturn(ntpGurl);
        when(regularNtpTab2.isNativePage()).thenReturn(true);
        when(regularNtpTab2.getTabGroupId()).thenReturn(new Token(1L, 2L));
        when(mNormalTabModel.getTabAt(1)).thenReturn(regularNtpTab2);

        // Regular selected tab.
        Tab regularTab1 = mock(Tab.class);
        GURL gurl = new GURL(REGULAR_TAB_STRING_1);
        when(regularTab1.getUrl()).thenReturn(gurl);
        when(mNormalTabModel.getTabAt(2)).thenReturn(regularTab1);

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
