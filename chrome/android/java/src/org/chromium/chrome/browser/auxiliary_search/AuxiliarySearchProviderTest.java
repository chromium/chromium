// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchTabGroup;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchProvider.FaviconImageFetchedCallback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/** Unit tests for {@link AuxiliarySearchProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION})
public class AuxiliarySearchProviderTest {
    private static final String TAB_TITLE = "tab";
    private static final String TAB_URL = "https://tab.google.com/";
    private static final String BOOKMARK_TITLE = "bookmark";
    private static final String BOOKMARK_URL = "https://bookmark.google.com";
    private static final String NEW_TAB_PAGE_URL = "chrome-native://newtab";
    private static final long FAKE_NATIVE_PROVIDER = 1;

    public @Rule JniMocker mJniMocker = new JniMocker();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock AuxiliarySearchBridge.Natives mMockAuxiliarySearchBridgeJni;
    private @Mock FaviconHelper.Natives mMockFaviconHelperJni;
    private @Mock Profile mProfile;
    private @Mock TabModelSelector mTabModelSelector;
    private @Mock FaviconImageFetchedCallback mFaviconImageFetchedCallback;
    private @Mock Callback<AuxiliarySearchTabGroup> mCallback;
    private @Mock FaviconHelper mFaviconHelper;
    private @Mock Context mContext;
    private @Mock Resources mResources;

    private AuxiliarySearchProvider mAuxiliarySearchProvider;
    private MockTabModel mMockNormalTabModel;

    @Before
    public void setUp() {
        mJniMocker.mock(AuxiliarySearchBridgeJni.TEST_HOOKS, mMockAuxiliarySearchBridgeJni);
        doReturn(FAKE_NATIVE_PROVIDER).when(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);
        when(mMockFaviconHelperJni.init()).thenReturn(1L);
        mJniMocker.mock(FaviconHelperJni.TEST_HOOKS, mMockFaviconHelperJni);

        when(mContext.getResources()).thenReturn(mResources);
        mAuxiliarySearchProvider =
                new AuxiliarySearchProvider(mContext, mProfile, mTabModelSelector);
        mMockNormalTabModel = new MockTabModel(mProfile, null);
        doReturn(mMockNormalTabModel).when(mTabModelSelector).getModel(false);
    }

    private Tab createTab(int index, long timestamp) {
        MockTab tab = mMockNormalTabModel.addTab(index);
        tab.setGurlOverrideForTesting(new GURL(TAB_URL + Integer.toString(index)));
        tab.setTitle(TAB_TITLE + Integer.toString(index));
        tab.setTimestampMillis(timestamp);

        return tab;
    }

    private void compareTabs(List<Tab> expectedTabs, List<Tab> returnedTabs) {
        HashSet<Integer> returnedTabsNumbers = new HashSet<Integer>();
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
    @SmallTest
    public void testGetBookmarksSearchableDataProto() {
        var bookmark =
                AuxiliarySearchEntry.newBuilder()
                        .setTitle(BOOKMARK_TITLE)
                        .setUrl(BOOKMARK_URL)
                        .setCreationTimestamp(1)
                        .build();
        var proto = AuxiliarySearchBookmarkGroup.newBuilder().addBookmark(bookmark).build();

        doReturn(proto.toByteArray())
                .when(mMockAuxiliarySearchBridgeJni)
                .getBookmarksSearchableData(FAKE_NATIVE_PROVIDER);

        AuxiliarySearchBookmarkGroup bookmarksList =
                mAuxiliarySearchProvider.getBookmarksSearchableDataProto();

        assertEquals(bookmarksList.getBookmarkCount(), 1);
        assertEquals(bookmarksList.getBookmark(0).getTitle(), BOOKMARK_TITLE);
        assertEquals(bookmarksList.getBookmark(0).getUrl(), BOOKMARK_URL);
        assertEquals(bookmarksList.getBookmark(0).getCreationTimestamp(), 1);
        assertFalse(bookmarksList.getBookmark(0).hasLastModificationTimestamp());
        assertFalse(bookmarksList.getBookmark(0).hasLastAccessTimestamp());
    }

    @Test
    @SmallTest
    public void testGetBookmarksSearchableDataProto_failureToParse() {
        // Return a random array which cannot been parsed to proto.
        doReturn(new byte[] {1, 2, 3})
                .when(mMockAuxiliarySearchBridgeJni)
                .getBookmarksSearchableData(FAKE_NATIVE_PROVIDER);

        AuxiliarySearchBookmarkGroup bookmarksList =
                mAuxiliarySearchProvider.getBookmarksSearchableDataProto();
        assertNull(bookmarksList);
    }

    @Test
    @SmallTest
    public void testGetTabsSearchableDataProtoAsync() {
        ArrayList<Tab> tabList = new ArrayList<>();
        long now = System.currentTimeMillis();
        // Create 200 tabs with different timestamps(from 0 to 199), and only the newest 100 tabs
        // should be returned from 'getTabsSearchableDataProtoWithFaviconAsync'.
        for (int i = 0; i < 200; i++) {
            MockTab tab = mMockNormalTabModel.addTab(i);
            tab.setGurlOverrideForTesting(new GURL(TAB_URL + Integer.toString(i)));
            tab.setTitle(TAB_TITLE + Integer.toString(i));
            tab.setTimestampMillis(now + i);
            if (i >= 100) {
                tabList.add(tab);
            }
        }

        Object[] tabObject = new Object[tabList.size()];
        tabList.toArray(tabObject);
        doAnswer(
                        invocation -> {
                            invocation.<Callback<Object[]>>getArgument(2).onResult(tabObject);
                            return null;
                        })
                .when(mMockAuxiliarySearchBridgeJni)
                .getNonSensitiveTabs(eq(FAKE_NATIVE_PROVIDER), any(), any(Callback.class));

        mAuxiliarySearchProvider.getTabsSearchableDataProtoWithFaviconAsync(
                new Callback<AuxiliarySearchTabGroup>() {
                    @Override
                    public void onResult(AuxiliarySearchTabGroup tabGroup) {
                        assertEquals(100, tabGroup.getTabCount());
                        HashSet<Integer> returnedTabsNumbers = new HashSet<Integer>();
                        for (int i = 0; i < tabGroup.getTabCount(); i++) {
                            AuxiliarySearchEntry tab = tabGroup.getTab(i);
                            assertTrue(tab.hasTitle());
                            assertTrue(tab.hasUrl());
                            assertTrue(tab.hasLastAccessTimestamp());
                            assertFalse(tab.hasCreationTimestamp());
                            assertFalse(tab.hasLastModificationTimestamp());

                            int number = Integer.valueOf(tab.getUrl().substring(TAB_URL.length()));
                            assertTrue(
                                    "Only the newest 100 tabs should be received",
                                    number >= 100 && number <= 199);
                            assertEquals(now + (long) number, tab.getLastAccessTimestamp());
                            returnedTabsNumbers.add(number);
                        }
                        assertEquals(returnedTabsNumbers.size(), 100);
                    }
                },
                null);
    }

    @Test
    @SmallTest
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
    @SmallTest
    public void testTabToAuxiliarySearchEntry_nullTab() {
        assertNull(AuxiliarySearchProvider.tabToAuxiliarySearchEntry(null));
    }

    @Test
    @SmallTest
    public void configuredTabsAgeCannotBeZero() {
        var config = new TestValues();
        config.addFieldTrialParamOverride(
                ChromeFeatureList.ANDROID_APP_INTEGRATION,
                AuxiliarySearchProvider.TAB_AGE_HOURS_PARAM,
                "0");
        FeatureList.setTestValues(config);
        // Recreate provider to update the finch parameter.
        mAuxiliarySearchProvider =
                new AuxiliarySearchProvider(mContext, mProfile, mTabModelSelector);

        assertNotEquals(0L, mAuxiliarySearchProvider.getTabsMaxAgeMs());
        assertEquals(
                AuxiliarySearchProvider.DEFAULT_TAB_AGE_HOURS * 60 * 60 * 1000,
                mAuxiliarySearchProvider.getTabsMaxAgeMs());
    }

    @Test
    @SmallTest
    public void configuredTabsAge() {
        var config = new TestValues();
        config.addFieldTrialParamOverride(
                ChromeFeatureList.ANDROID_APP_INTEGRATION,
                AuxiliarySearchProvider.TAB_AGE_HOURS_PARAM,
                "10");
        FeatureList.setTestValues(config);
        // Recreate provider to update the finch parameter.
        mAuxiliarySearchProvider =
                new AuxiliarySearchProvider(mContext, mProfile, mTabModelSelector);
        assertEquals(10 * 60 * 60 * 1000, mAuxiliarySearchProvider.getTabsMaxAgeMs());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON)
    public void testOnNonSensitiveTabsAvailable() {
        int maxFaviconNumber = 5;
        AuxiliarySearchProvider.MAX_FAVICON_NUMBER.setForTesting(maxFaviconNumber);
        assertEquals(maxFaviconNumber, AuxiliarySearchProvider.MAX_FAVICON_NUMBER.getValue());

        ArrayList<Tab> tabList = new ArrayList<>();
        int count = 100;
        long now = System.currentTimeMillis();
        // The tabs are added with timestamps ascending, i.e., the most recent tab added at the
        // end.
        for (int i = 0; i < count; i++) {
            MockTab tab = mMockNormalTabModel.addTab(i);
            tab.setGurlOverrideForTesting(new GURL(TAB_URL + Integer.toString(i)));
            tab.setTitle(TAB_TITLE + Integer.toString(i));
            tab.setTimestampMillis(now + i);
            tabList.add(tab);
        }

        mAuxiliarySearchProvider.onNonSensitiveTabsAvailable(
                mFaviconHelper, mCallback, mFaviconImageFetchedCallback, tabList);

        // Verifies that the tabs are sorted with timestamp descending, i.e., the most recent tab as
        // as the first.
        for (int i = 0; i < count; i++) {
            Tab tab = tabList.get(i);
            assertEquals(now + count - i - 1, tab.getTimestampMillis());
            assertEquals(TAB_TITLE + Integer.toString(count - i - 1), tab.getTitle());
        }
        verify(mFaviconHelper, times(maxFaviconNumber))
                .getLocalFaviconImageForURL(
                        any(Profile.class),
                        any(GURL.class),
                        anyInt(),
                        any(FaviconImageCallback.class));
    }
}
