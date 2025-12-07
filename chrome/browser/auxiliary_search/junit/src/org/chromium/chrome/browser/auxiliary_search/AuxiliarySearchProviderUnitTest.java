// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchController.AuxiliarySearchHostType;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchProvider.MetaDataVersion;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.io.File;
import java.util.HashSet;
import java.util.List;

/** Unit tests for {@link AuxiliarySearchProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AuxiliarySearchProviderUnitTest {
    private static final String TAB_TITLE = "tab";
    private static final String TAB_URL = "https://tab.google.com/";
    private static final long FAKE_NATIVE_PROVIDER = 1;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock AuxiliarySearchBridge.Natives mMockAuxiliarySearchBridgeJni;
    private @Mock FaviconHelper.Natives mMockFaviconHelperJni;
    private @Mock Profile mProfile;
    private @Mock TabModelSelector mTabModelSelector;
    private @Mock Context mContext;
    private @Mock Resources mResources;
    private @Mock BackgroundTaskScheduler mBackgroundTaskScheduler;

    private AuxiliarySearchProvider mAuxiliarySearchProvider;
    private MockTabModel mMockNormalTabModel;

    @Before
    public void setUp() {
        AuxiliarySearchBridgeJni.setInstanceForTesting(mMockAuxiliarySearchBridgeJni);
        doReturn(FAKE_NATIVE_PROVIDER).when(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);
        when(mMockFaviconHelperJni.init()).thenReturn(1L);
        FaviconHelperJni.setInstanceForTesting(mMockFaviconHelperJni);

        when(mContext.getResources()).thenReturn(mResources);
        mAuxiliarySearchProvider =
                new AuxiliarySearchProvider(
                        mContext, mProfile, mTabModelSelector, AuxiliarySearchHostType.CTA);
        mMockNormalTabModel = new MockTabModel(mProfile, null);
        doReturn(mMockNormalTabModel).when(mTabModelSelector).getModel(false);

        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mBackgroundTaskScheduler);
    }

    @After
    public void tearDown() {
        File tabDonateFile = AuxiliarySearchUtils.getTabDonateFile(mContext);
        if (tabDonateFile.exists()) {
            boolean deleteResult = tabDonateFile.delete();
            assertTrue(deleteResult);
        }
    }

    private Tab createTab(int index, long timestamp) {
        MockTab tab = mMockNormalTabModel.addTab(index);
        tab.setGurlOverrideForTesting(new GURL(TAB_URL + Integer.toString(index)));
        tab.setTitle(TAB_TITLE + Integer.toString(index));
        tab.setTimestampMillis(timestamp);

        return tab;
    }

    private void compareTabs(List<Tab> expectedTabs, List<Tab> returnedTabs) {
        HashSet<Integer> returnedTabsNumbers = new HashSet<>();
        for (Tab returnedTab : returnedTabs) {
            int returnedNumber =
                    Integer.valueOf(returnedTab.getUrl().getSpec().substring(TAB_URL.length()));

            Tab expectedTab = null;
            for (Tab tab : expectedTabs) {
                int expectedNumber =
                        Integer.valueOf(tab.getUrl().getSpec().substring(TAB_URL.length()));
                if (expectedNumber == returnedNumber) {
                    expectedTab = tab;
                    break;
                }
            }
            assertNotNull(expectedTab);
            assertEquals(returnedTab.getTitle(), expectedTab.getTitle());
            assertEquals(returnedTab.getTitle(), expectedTab.getTitle());
            assertEquals(returnedTab.getTimestampMillis(), expectedTab.getTimestampMillis());

            returnedTabsNumbers.add(returnedNumber);
        }
        assertEquals(expectedTabs.size(), returnedTabsNumbers.size());
    }

    @Test
    public void testGetTabsByMinimalAccessTime() {
        long now = System.currentTimeMillis();
        List<Tab> tabList =
                List.of(
                        createTab(1, now - 50),
                        createTab(2, now - 100),
                        createTab(3, now - 150),
                        createTab(4, now - 200),
                        createTab(5, now - 250));

        var tabs = mAuxiliarySearchProvider.getTabsByMinimalAccessTime(now - 150);
        assertEquals(3, tabs.size());
        compareTabs(tabList.subList(0, 3), tabs);

        tabs = mAuxiliarySearchProvider.getTabsByMinimalAccessTime(now);
        assertEquals(0, tabs.size());

        tabs = mAuxiliarySearchProvider.getTabsByMinimalAccessTime(0);
        assertEquals(5, tabs.size());
        compareTabs(tabList, tabs);
    }

    @Test
    public void testScheduleBackgroundTask() {
        long expectedStartTimeMs = 1000;

        TaskInfo taskInfo = mAuxiliarySearchProvider.scheduleBackgroundTask(expectedStartTimeMs);
        var bundle = taskInfo.getExtras();
        assertEquals(
                expectedStartTimeMs, bundle.getLong(AuxiliarySearchProvider.TASK_CREATED_TIME));
        assertFalse(taskInfo.isUserInitiated());
        assertTrue(taskInfo.shouldUpdateCurrent());
        assertTrue(taskInfo.isPersisted());
    }

    @Test
    public void testSaveAndReadDonationMetadataAsync_V1() {
        @MetaDataVersion int metaDataVersion = MetaDataVersion.V1;
        long now = TimeUtils.uptimeMillis();
        List<AuxiliarySearchEntry> entries =
                AuxiliarySearchTestHelper.createAuxiliarySearchEntries(now);

        testSaveAndReadDonationMetadataAsyncImpl(
                entries,
                metaDataVersion,
                (entryList) -> {
                    assertEquals(1, entryList.size());

                    AuxiliarySearchEntry entry = (AuxiliarySearchEntry) entryList.get(0);
                    assertEquals(AuxiliarySearchTestHelper.TAB_ID_2, entry.getId());
                    assertEquals(JUnitTestGURLs.URL_2.getSpec(), entry.getUrl());
                    assertEquals(AuxiliarySearchTestHelper.TITLE_2, entry.getTitle());
                    assertEquals(now, entry.getLastAccessTimestamp());
                });
    }

    @Test
    public void testSaveAndReadDonationMetadataAsync_V2() {
        @MetaDataVersion int metaDataVersion = MetaDataVersion.MULTI_TYPE_V2;
        long now = TimeUtils.uptimeMillis();
        // The entries include 2 elements, and the second one will be saved to the metadata file.
        // This is because testSaveAndReadDonationMetadataAsyncImpl() sets the starting index to 1.
        List<AuxiliarySearchDataEntry> entries =
                AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries(now);

        testSaveAndReadDonationMetadataAsyncImpl(
                entries,
                metaDataVersion,
                (entryList) -> {
                    assertEquals(1, entryList.size());

                    AuxiliarySearchDataEntry entry = (AuxiliarySearchDataEntry) entryList.get(0);
                    assertEquals(AuxiliarySearchEntryType.TAB, entry.type);
                    assertEquals(AuxiliarySearchTestHelper.TAB_ID_2, entry.tabId);
                    assertEquals(JUnitTestGURLs.URL_2, entry.url);
                    assertEquals(AuxiliarySearchTestHelper.TITLE_2, entry.title);
                    assertEquals(now, entry.lastActiveTime);
                    assertNull(entry.appId);
                    assertEquals(Tab.INVALID_TAB_ID, entry.visitId);
                });
    }

    @Test
    public void testSaveAndReadDonationMetadataAsync_V2_TopSite() {
        @MetaDataVersion int metaDataVersion = MetaDataVersion.MULTI_TYPE_V2;
        long now = TimeUtils.uptimeMillis();
        // The entries include 2 elements, and the second one will be saved to the metadata file.
        // This is because testSaveAndReadDonationMetadataAsyncImpl() sets the starting index to 1.
        List<AuxiliarySearchDataEntry> entries =
                AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries_TopSite(now);

        testSaveAndReadDonationMetadataAsyncImpl(
                entries,
                metaDataVersion,
                (entryList) -> {
                    assertEquals(1, entryList.size());

                    AuxiliarySearchDataEntry entry = (AuxiliarySearchDataEntry) entryList.get(0);
                    assertEquals(AuxiliarySearchEntryType.TOP_SITE, entry.type);
                    assertEquals(Tab.INVALID_TAB_ID, entry.tabId);
                    assertEquals(JUnitTestGURLs.URL_2, entry.url);
                    assertEquals(AuxiliarySearchTestHelper.TITLE_2, entry.title);
                    assertEquals(now, entry.lastActiveTime);
                    assertNull(entry.appId);
                    assertEquals(AuxiliarySearchTestHelper.VISIT_ID_2, entry.visitId);
                    assertEquals(AuxiliarySearchTestHelper.SCORE_2, entry.score);
                });
    }

    @Test
    public void testSaveAndReadDonationMetadataAsync_V2_CustomTabs() {
        @MetaDataVersion int metaDataVersion = MetaDataVersion.MULTI_TYPE_V2;
        long now = TimeUtils.uptimeMillis();
        // The entries include 3 elements, and the second and third ones will be saved to the
        // metadata file. This is because testSaveAndReadDonationMetadataAsyncImpl() sets the
        // starting index to 1.
        List<AuxiliarySearchDataEntry> entries =
                AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries_CustomTabs(now);

        testSaveAndReadDonationMetadataAsyncImpl(
                entries,
                metaDataVersion,
                (entryList) -> {
                    assertEquals(2, entryList.size());

                    AuxiliarySearchDataEntry entry = (AuxiliarySearchDataEntry) entryList.get(0);
                    assertEquals(AuxiliarySearchEntryType.CUSTOM_TAB, entry.type);
                    assertEquals(Tab.INVALID_TAB_ID, entry.tabId);
                    assertEquals(JUnitTestGURLs.URL_2, entry.url);
                    assertEquals(AuxiliarySearchTestHelper.TITLE_2, entry.title);
                    assertEquals(now, entry.lastActiveTime);
                    // Verifies the case with an empty string as app id.
                    assertEquals("", entry.appId);
                    assertEquals(AuxiliarySearchTestHelper.VISIT_ID_2, entry.visitId);
                    assertEquals(0, entry.score);

                    // Verifies the case with an non-empty app id.
                    entry = (AuxiliarySearchDataEntry) entryList.get(1);
                    assertEquals(AuxiliarySearchTestHelper.APP_ID_2, entry.appId);
                });
    }

    @Test
    public void testCreationViaCTABackgroundTask() {
        mAuxiliarySearchProvider =
                new AuxiliarySearchProvider(
                        mContext,
                        mProfile,
                        mTabModelSelector,
                        AuxiliarySearchHostType.BACKGROUND_TASK);
        assertTrue(mAuxiliarySearchProvider.isAuxiliarySearchBridgeNullForTesting());
    }

    @Test
    public void testGetCustomTabsAsync() {
        mAuxiliarySearchProvider =
                new AuxiliarySearchProvider(
                        mContext, mProfile, mTabModelSelector, AuxiliarySearchHostType.CUSTOM_TAB);
        assertFalse(mAuxiliarySearchProvider.isAuxiliarySearchBridgeNullForTesting());

        long beginTime = 10;
        Callback<@Nullable List<AuxiliarySearchDataEntry>> callback = mock(Callback.class);
        GURL url = JUnitTestGURLs.URL_1;
        mAuxiliarySearchProvider.getCustomTabsAsync(url, beginTime, callback);

        verify(mMockAuxiliarySearchBridgeJni)
                .getCustomTabs(eq(FAKE_NATIVE_PROVIDER), eq(url), eq(beginTime), eq(callback));
    }

    private <T> void testSaveAndReadDonationMetadataAsyncImpl(
            List<T> entries, int metaDataVersion, Callback<List<T>> callback) {
        int startIndex = 1;
        int remainingEntryCount = 1;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAuxiliarySearchProvider.saveTabMetadataToFile(
                            AuxiliarySearchUtils.getTabDonateFile(mContext),
                            metaDataVersion,
                            entries,
                            startIndex,
                            remainingEntryCount);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> AuxiliarySearchBackgroundTask.readDonationMetadataAsync(mContext, callback));
    }
}
