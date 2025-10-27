// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockingDetails;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.text.TextUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.UserDataHost;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl.TabRestoreDetails;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for the tab persistent store logic. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TabPersistentStoreUnitTest {
    private static final @TabId int RESTORE_TAB_ID_1 = 31;
    private static final @TabId int RESTORE_TAB_ID_2 = 32;
    private static final @TabId int RESTORE_TAB_ID_3 = 33;

    private static final String REGULAR_TAB_STRING_1 = "https://foo.com/";
    private static final String INCOGNITO_TAB_STRING_1 = "https://bar.com/";
    private static final String INCOGNITO_TAB_STRING_2 = "https://baz.com/";
    private static final String RESTORE_TAB_STRING_1 = "https://qux.com/";
    private static final String RESTORE_TAB_STRING_2 = "https://quux.com/";
    private static final String RESTORE_TAB_STRING_3 = "https://quuz.com/";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabPersistencePolicy mPersistencePolicy;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModelInternal mNormalTabModel;
    @Mock private TabModelInternal mIncognitoTabModel;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabCreator mNormalTabCreator;
    @Mock private TabCreator mIncognitoTabCreator;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabGroupModelFilter mNormalTabGroupModelFilter;
    @Mock private TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Mock private SequencedTaskRunner mSequencedTaskRunner;
    @Mock private Tab mTab;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private TabPersistentStoreImpl mPersistentStore;
    private CipherFactory mCipherFactory;

    @Before
    public void setUp() {
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mIncognitoTabModel.iterator()).thenAnswer(inv -> Collections.emptyList().iterator());
        when(mNormalTabModel.iterator()).thenAnswer(inv -> Collections.emptyList().iterator());
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        when(mTabCreatorManager.getTabCreator(false)).thenReturn(mNormalTabCreator);
        when(mTabCreatorManager.getTabCreator(true)).thenReturn(mIncognitoTabCreator);

        when(mPersistencePolicy.getMetadataFileName())
                .thenReturn(TabMetadataFileManager.SAVED_METADATA_FILE_PREFIX + "state_files_yay");
        when(mPersistencePolicy.isMergeInProgress()).thenReturn(false);
        when(mPersistencePolicy.performInitialization(any(TaskRunner.class))).thenReturn(false);

        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .thenReturn(mNormalTabGroupModelFilter);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(true))
                .thenReturn(mIncognitoTabGroupModelFilter);

        mCipherFactory = new CipherFactory();
    }

    @After
    public void tearDown() throws Exception {
        // Flush pending PersistentStore tasks.
        final AtomicBoolean flushed = new AtomicBoolean(false);
        if (mPersistentStore != null) {
            SequencedTaskRunner runner = mPersistentStore.getTaskRunnerForTesting();
            if (!mockingDetails(runner).isMock()) {
                runner.execute(() -> flushed.set(true));
                BaseRobolectricTestRule.runAllBackgroundAndUi();
                assertThat(flushed.get()).isTrue();
            }
        }
    }

    @Test
    @Feature("TabPersistentStore")
    public void testNtpSaveBehavior() {
        when(mNormalTabModel.index()).thenReturn(TabList.INVALID_TAB_INDEX);
        when(mIncognitoTabModel.index()).thenReturn(TabList.INVALID_TAB_INDEX);

        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
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
        TabStateAttributes.from(emptyNtpTab).setStateForTesting(DirtinessState.DIRTY);

        mPersistentStore.addTabToSaveQueue(emptyNtpTab);
        assertTrue(mPersistentStore.isTabPendingSave(emptyNtpTab));
    }

    @Test
    @Feature("TabPersistentStore")
    public void testNotActiveEmptyNtpNotIgnoredDuringRestore() {
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
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
    @Feature("TabPersistentStore")
    public void testActiveEmptyNtpNotIgnoredDuringRestore() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);

        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
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
    @Feature("TabPersistentStore")
    public void testNtpFromMergeWithNoStateNotIgnoredDuringMerge() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);

        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
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
    @Feature("TabPersistentStore")
    public void testNtpWithStateNotIgnoredDuringRestore() {
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
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
    @Feature("TabPersistentStore")
    public void testActiveEmptyIncognitoNtpNotIgnoredDuringRestore() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);

        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
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
    @Feature("TabPersistentStore")
    public void testReparentedTabNotIgnoredDuringRestore() {
        String url = "https://test.com";
        AsyncTabParamsManagerSingleton.getInstance()
                .add(1, new AsyncTabCreationParams(new LoadUrlParams(url)));
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.initializeRestoreVars(false);

        TabRestoreDetails emptyNtpDetails = new TabRestoreDetails(1, 0, false, url, false);
        mPersistentStore.restoreTab(emptyNtpDetails, null, false);

        verify(mNormalTabCreator)
                .createNewTab(
                        argThat(new LoadUrlParamsUrlMatcher(url)),
                        eq(TabLaunchType.FROM_RESTORE),
                        isNull(),
                        eq(0));
        AsyncTabParamsManagerSingleton.getInstance().remove(1);
    }

    @Test
    @Feature("TabPersistentStore")
    public void testNotActiveIncognitoNtpIgnoredDuringRestore() {
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
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
    @Feature("TabPersistentStore")
    public void testActiveEmptyIncognitoNtpIgnoredDuringRestoreIfIncognitoLoadingIsDisabled() {
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
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
    @Feature("TabPersistentStore")
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_DEDUPE_TAB_IDS_KILL_SWITCH)
    public void testDuplicateTabIds() {
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.initializeRestoreVars(false);
        when(mNormalTabCreator.createFrozenTab(any(), anyInt(), anyInt())).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(new GURL(RESTORE_TAB_STRING_1));

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
    @Feature("TabPersistentStore")
    public void testSerializeTabModelSelector() {
        setupSerializationTestMocks();
        TabModelSelectorMetadata metadata =
                TabPersistentStoreImpl.extractTabMetadataFromSelector(mTabModelSelector, null);

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
    @Feature("TabPersistentStore")
    public void testSkipNonActiveNtpsWithSkippedNtpComeBeforeActiveTab() {
        setupSerializationTestMocksWithSkippedNtpComeBeforeActiveTab();
        TabModelSelectorMetadata metadata =
                TabPersistentStoreImpl.extractTabMetadataFromSelector(mTabModelSelector, null);

        assertEquals("Incorrect index for regular", 0, metadata.normalModelMetadata.index);
        assertEquals(
                "Incorrect number of tabs in regular", 1, metadata.normalModelMetadata.ids.size());
        assertEquals(
                "Incorrect URL for regular tab.",
                REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(0));
    }

    @Test
    @Feature("TabPersistentStore")
    public void testSkipNonActiveNtpsWithSkippedNtpComeAfterActiveTab() {
        setupSerializationTestMocks();
        TabModelSelectorMetadata metadata =
                TabPersistentStoreImpl.extractTabMetadataFromSelector(mTabModelSelector, null);

        assertEquals("Incorrect index for regular", 0, metadata.normalModelMetadata.index);
        assertEquals(
                "Incorrect number of tabs in regular", 1, metadata.normalModelMetadata.ids.size());
        assertEquals(
                "Incorrect URL for regular tab.",
                REGULAR_TAB_STRING_1,
                metadata.normalModelMetadata.urls.get(0));
    }

    @Test
    @Feature("TabPersistentStore")
    public void testSkipNonActiveNtpsWithGroupedAndNavigableNtps_TabGroupStableIdsEnabled() {
        setupSerializationTestMocksWithGroupedAndNavigableNtps();
        TabModelSelectorMetadata metadata =
                TabPersistentStoreImpl.extractTabMetadataFromSelector(mTabModelSelector, null);

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
    @Feature("TabPersistentStore")
    public void testSerializeTabModelSelector_tabsBeingRestored() {
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
                TabPersistentStoreImpl.extractTabMetadataFromSelector(
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
    @Feature("TabPersistentStore")
    public void testSerializeTabModelSelector_closingTabsSkipped() {
        when(mNormalTabModel.getCount()).thenReturn(2);
        when(mNormalTabModel.index()).thenReturn(1);
        Tab regularTab1 = mock(Tab.class);
        when(regularTab1.getId()).thenReturn(11);
        when(regularTab1.getUrl()).thenReturn(new GURL(REGULAR_TAB_STRING_1));
        when(regularTab1.isClosing()).thenReturn(false);
        when(mNormalTabModel.getTabAtChecked(0)).thenReturn(regularTab1);
        Tab regularTab2 = mock(Tab.class);
        when(regularTab2.getId()).thenReturn(22);
        when(regularTab2.getUrl()).thenReturn(new GURL(RESTORE_TAB_STRING_2));
        when(regularTab2.isClosing()).thenReturn(true);
        when(mNormalTabModel.getTabAtChecked(1)).thenReturn(regularTab2);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        when(mNormalTabModel.iterator())
                .thenAnswer(inv -> List.of(regularTab1, regularTab2).iterator());

        TabModelSelectorMetadata metadata =
                TabPersistentStoreImpl.extractTabMetadataFromSelector(mTabModelSelector, null);

        assertEquals(1, metadata.normalModelMetadata.ids.size());
        assertEquals(1, metadata.normalModelMetadata.urls.size());
        assertEquals(0, metadata.normalModelMetadata.index);
        assertEquals(11, metadata.normalModelMetadata.ids.get(0).intValue());
        assertEquals(REGULAR_TAB_STRING_1, metadata.normalModelMetadata.urls.get(0));
    }

    @Test
    @Feature("TabPersistentStore")
    @DisableFeatures(ChromeFeatureList.TAB_MODEL_INIT_FIXES)
    public void testTabModelObserver_withoutInitFeature() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);
        when(mNormalTabModel.getTabAtChecked(anyInt())).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.setSequencedTaskRunnerForTesting(mSequencedTaskRunner);
        mPersistentStore.onNativeLibraryReady();
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab, TabSelectionType.FROM_USER, TabModel.INVALID_TAB_INDEX);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.FROZEN_ON_RESTORE,
                        /* markedForSelection= */ false);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);
    }

    @Test
    @Feature("TabPersistentStore")
    @EnableFeatures(ChromeFeatureList.TAB_MODEL_INIT_FIXES)
    public void testTabModelObserver_beforeAndAfterInit() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);
        when(mNormalTabModel.getTabAtChecked(anyInt())).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.setSequencedTaskRunnerForTesting(mSequencedTaskRunner);
        mPersistentStore.onNativeLibraryReady();
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());

        // Before tab model restore, these signals should be ignored.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab, TabSelectionType.FROM_USER, TabModel.INVALID_TAB_INDEX);
        verify(mSequencedTaskRunner, never()).execute(any());

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.FROZEN_ON_RESTORE,
                        /* markedForSelection= */ false);
        verify(mSequencedTaskRunner, never()).execute(any());

        // Now they should all trigger saves.
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab, TabSelectionType.FROM_USER, TabModel.INVALID_TAB_INDEX);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.FROZEN_ON_RESTORE,
                        /* markedForSelection= */ false);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);
    }

    @Test
    @Feature("TabPersistentStore")
    @EnableFeatures(ChromeFeatureList.TAB_MODEL_INIT_FIXES)
    public void testTabModelObserver_nonInitEvents() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);
        when(mNormalTabModel.getTabAtChecked(anyInt())).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.setSequencedTaskRunnerForTesting(mSequencedTaskRunner);
        mPersistentStore.onNativeLibraryReady();
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 0);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab,
                        TabLaunchType.FROM_LINK,
                        TabCreationState.FROZEN_ON_RESTORE,
                        /* markedForSelection= */ false);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);
    }

    @Test
    @Feature("TabPersistentStore")
    @DisableFeatures(ChromeFeatureList.TAB_MODEL_INIT_FIXES)
    public void testPauseSaveTabList() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);
        when(mNormalTabModel.getTabAtChecked(anyInt())).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.setSequencedTaskRunnerForTesting(mSequencedTaskRunner);
        mPersistentStore.onNativeLibraryReady();
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        observer.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 0);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);

        mPersistentStore.pauseSaveTabList();
        observer.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 0);
        verify(mSequencedTaskRunner, never()).execute(any());

        mPersistentStore.resumeSaveTabList(() -> {});
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);

        observer.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 0);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);
    }

    @Test
    @Feature("TabPersistentStore")
    @EnableFeatures(ChromeFeatureList.TAB_MODEL_INIT_FIXES)
    public void testPauseSaveTabList_OnlySavesWhenDirty() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);
        when(mNormalTabModel.getTabAtChecked(anyInt())).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.setSequencedTaskRunnerForTesting(mSequencedTaskRunner);
        mPersistentStore.onNativeLibraryReady();
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        observer.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 0);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);

        mPersistentStore.pauseSaveTabList();
        observer.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 0);
        verify(mSequencedTaskRunner, never()).execute(any());

        mPersistentStore.resumeSaveTabList(() -> {});
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);

        observer.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 0);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);

        mPersistentStore.pauseSaveTabList();
        mPersistentStore.resumeSaveTabList(() -> {});
        verify(mSequencedTaskRunner, never()).execute(any());

        observer.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 0);
        verify(mSequencedTaskRunner).execute(any());
        reset(mSequencedTaskRunner);
    }

    @Test
    @Feature("TabPersistentStore")
    @EnableFeatures(ChromeFeatureList.TAB_MODEL_INIT_FIXES)
    public void testSaveState_currentTabDirtyCleared() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mNormalTabModel);
        when(mNormalTabModel.getTabAtChecked(anyInt())).thenReturn(mTab);
        when(mNormalTabModel.getTabAt(anyInt())).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        mPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
                        mPersistencePolicy,
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabWindowManager,
                        mCipherFactory);
        mPersistentStore.setSequencedTaskRunnerForTesting(mSequencedTaskRunner);

        UserDataHost userDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(userDataHost);
        TabStateAttributes.createForTab(mTab, TabCreationState.LIVE_IN_FOREGROUND);
        TabStateAttributes.from(mTab).updateIsDirty(DirtinessState.UNTIDY);
        assertEquals(DirtinessState.UNTIDY, TabStateAttributes.from(mTab).getDirtinessState());

        mPersistentStore.saveState();

        assertEquals(DirtinessState.CLEAN, TabStateAttributes.from(mTab).getDirtinessState());
    }

    @Test
    @Feature({"TabPersistentStore"})
    public void testShouldSkipTab_PinnedNtpIsNotSkipped() {
        GURL ntpGurl = new GURL(UrlConstants.NTP_URL);
        GURL regularGurl = new GURL(REGULAR_TAB_STRING_1);

        // Pinned NTPs should not be skipped.
        when(mTab.getUrl()).thenReturn(ntpGurl);
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getIsPinned()).thenReturn(true);
        assertFalse("Pinned NTPs should not be skipped.", TabPersistenceUtils.shouldSkipTab(mTab));

        // Pinned regular tabs should not be skipped.
        when(mTab.getUrl()).thenReturn(regularGurl);
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.getIsPinned()).thenReturn(true);
        assertFalse(
                "Pinned regular tabs should not be skipped.",
                TabPersistenceUtils.shouldSkipTab(mTab));
    }

    private void setupSerializationTestMocks() {
        when(mNormalTabModel.getCount()).thenReturn(2);
        when(mNormalTabModel.index()).thenReturn(0);
        Tab regularTab1 = mock(Tab.class);
        GURL gurl = new GURL(REGULAR_TAB_STRING_1);
        when(regularTab1.getUrl()).thenReturn(gurl);
        when(mNormalTabModel.getTabAtChecked(0)).thenReturn(regularTab1);

        Tab regularNtpTab1 = mock(Tab.class);
        GURL ntpGurl = new GURL(UrlConstants.NTP_URL);
        when(regularNtpTab1.getUrl()).thenReturn(ntpGurl);
        when(regularNtpTab1.isNativePage()).thenReturn(true);
        when(mNormalTabModel.getTabAtChecked(1)).thenReturn(regularNtpTab1);
        when(mNormalTabModel.iterator())
                .thenAnswer(inv -> List.of(regularTab1, regularNtpTab1).iterator());

        when(mIncognitoTabModel.getCount()).thenReturn(2);
        when(mIncognitoTabModel.index()).thenReturn(1);
        Tab incognitoTab1 = mock(Tab.class);
        gurl = new GURL(INCOGNITO_TAB_STRING_1);
        when(incognitoTab1.getUrl()).thenReturn(gurl);
        when(incognitoTab1.isIncognito()).thenReturn(true);
        when(mIncognitoTabModel.getTabAtChecked(0)).thenReturn(incognitoTab1);

        Tab incognitoTab2 = mock(Tab.class);
        gurl = new GURL(INCOGNITO_TAB_STRING_2);
        when(incognitoTab2.getUrl()).thenReturn(gurl);
        when(incognitoTab2.isIncognito()).thenReturn(true);
        when(mIncognitoTabModel.getTabAtChecked(1)).thenReturn(incognitoTab2);
        when(mIncognitoTabModel.iterator())
                .thenAnswer(inv -> List.of(incognitoTab1, incognitoTab2).iterator());

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
        when(mNormalTabModel.getTabAtChecked(0)).thenReturn(regularNtpTab1);

        Tab regularTab1 = mock(Tab.class);
        GURL gurl = new GURL(REGULAR_TAB_STRING_1);
        when(regularTab1.getUrl()).thenReturn(gurl);
        when(mNormalTabModel.getTabAtChecked(1)).thenReturn(regularTab1);
        when(mNormalTabModel.iterator())
                .thenAnswer(inv -> List.of(regularNtpTab1, regularTab1).iterator());

        when(mIncognitoTabModel.getCount()).thenReturn(0);
        when(mIncognitoTabModel.iterator()).thenAnswer(inv -> Collections.emptyList().iterator());
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
        when(mNormalTabModel.getTabAtChecked(0)).thenReturn(regularNtpTab1);

        // Non-active NTP with tab group.
        Tab regularNtpTab2 = mock(Tab.class);
        when(regularNtpTab2.getId()).thenReturn(1);
        when(regularNtpTab2.getUrl()).thenReturn(ntpGurl);
        when(regularNtpTab2.isNativePage()).thenReturn(true);
        when(regularNtpTab2.getTabGroupId()).thenReturn(new Token(1L, 2L));
        when(mNormalTabModel.getTabAtChecked(1)).thenReturn(regularNtpTab2);

        // Regular selected tab.
        Tab regularTab1 = mock(Tab.class);
        GURL gurl = new GURL(REGULAR_TAB_STRING_1);
        when(regularTab1.getUrl()).thenReturn(gurl);
        when(mNormalTabModel.getTabAtChecked(2)).thenReturn(regularTab1);
        when(mNormalTabModel.iterator())
                .thenAnswer(inv -> List.of(regularNtpTab1, regularNtpTab2, regularTab1).iterator());

        when(mIncognitoTabModel.getCount()).thenReturn(0);
        when(mIncognitoTabModel.iterator()).thenAnswer(inv -> Collections.emptyList().iterator());
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
