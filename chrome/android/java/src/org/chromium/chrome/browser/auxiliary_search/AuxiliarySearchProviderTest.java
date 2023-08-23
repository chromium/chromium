// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchTabGroup;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashSet;

/**
 * Unit tests for {@link AuxiliarySearchProvider}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION})
@DisableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION_SAFE_SEARCH})
public class AuxiliarySearchProviderTest {
    private static final String TAB_TITLE = "tab";
    private static final String TAB_URL = "https://tab.google.com/";
    private static final String BOOKMARK_TITLE = "bookmark";
    private static final String BOOKMARK_URL = "https://bookmark.google.com";
    private static final String NEW_TAB_PAGE_URL = "chrome-native://newtab";
    private static final long FAKE_NATIVE_PROVIDER = 1;

    public @Rule JniMocker mJniMocker = new JniMocker();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule TestRule mProcessor = new Features.JUnitProcessor();

    private @Mock AuxiliarySearchBridge.Natives mMockAuxiliarySearchBridgeJni;
    private @Mock Profile mProfile;
    private @Mock TabModelSelector mTabModelSelector;

    private AuxiliarySearchProvider mAuxiliarySearchProvider;

    @Before
    public void setUp() {
        mJniMocker.mock(AuxiliarySearchBridgeJni.TEST_HOOKS, mMockAuxiliarySearchBridgeJni);
        doReturn(FAKE_NATIVE_PROVIDER).when(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);
        doAnswer((Answer<Object[]>) invocation -> ((Object[]) invocation.getArguments()[1]))
                .when(mMockAuxiliarySearchBridgeJni)
                .getSearchableTabs(eq(FAKE_NATIVE_PROVIDER), any(Tab[].class));
        mAuxiliarySearchProvider = new AuxiliarySearchProvider(mProfile, mTabModelSelector);
    }

    @Test
    @SmallTest
    public void testGetTabsSearchableDataProto() throws InterruptedException {
        MockTabModel mockTabModel = new MockTabModel(false, null);
        for (int i = 0; i < 200; i++) {
            MockTab tab = (MockTab) mockTabModel.addTab(i);
            tab.setGurlOverrideForTesting(new GURL(TAB_URL + Integer.toString(i)));
            CriticalPersistedTabData.from(tab).setTitle(TAB_TITLE + Integer.toString(i));
            CriticalPersistedTabData.from(tab).setTimestampMillis(i);
        }

        doReturn(mockTabModel).when(mTabModelSelector).getModel(false);
        AuxiliarySearchTabGroup tabGroup = mAuxiliarySearchProvider.getTabsSearchableDataProto();

        assertEquals(tabGroup.getTabCount(), 100);
        HashSet<Integer> returnedTabsNumbers = new HashSet<Integer>();
        for (int i = 0; i < tabGroup.getTabCount(); i++) {
            AuxiliarySearchEntry tab = tabGroup.getTab(i);
            assertTrue(tab.hasTitle());
            assertTrue(tab.hasUrl());
            assertTrue(tab.hasLastAccessTimestamp());
            assertFalse(tab.hasCreationTimestamp());
            assertFalse(tab.hasLastModificationTimestamp());

            int number = Integer.valueOf(tab.getUrl().substring(TAB_URL.length()));
            assertTrue(number >= 100 && number <= 199);
            assertEquals(number, (int) tab.getLastAccessTimestamp());
            returnedTabsNumbers.add(number);
        }
        assertEquals(returnedTabsNumbers.size(), 100);
    }

    @Test
    @SmallTest
    public void testGetBookmarksSearchableDataProto() {
        var bookmark = AuxiliarySearchEntry.newBuilder()
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
    public void testTabHasNullTitle() {
        MockTabModel mockTabModel = new MockTabModel(false, null);

        // Add a normal tab
        MockTab tab = (MockTab) mockTabModel.addTab(0);
        tab.setGurlOverrideForTesting(new GURL(TAB_URL + "0"));
        CriticalPersistedTabData.from(tab).setTitle(TAB_TITLE + "0");
        CriticalPersistedTabData.from(tab).setTimestampMillis(0);

        // Add a null title tab
        tab = (MockTab) mockTabModel.addTab(1);
        tab.setGurlOverrideForTesting(new GURL(TAB_URL + Integer.toString(1)));
        CriticalPersistedTabData.from(tab).setTimestampMillis(1);
        CriticalPersistedTabData.from(tab).setTitle(null);

        doReturn(mockTabModel).when(mTabModelSelector).getModel(false);
        AuxiliarySearchTabGroup tabGroup = mAuxiliarySearchProvider.getTabsSearchableDataProto();

        assertEquals(1, tabGroup.getTabCount());
        assertTrue(tabGroup.getTab(0).hasTitle());
        assertEquals(TAB_TITLE + "0", tabGroup.getTab(0).getTitle());
        assertTrue(tabGroup.getTab(0).hasUrl());
        assertEquals(TAB_URL + "0", tabGroup.getTab(0).getUrl());
    }

    @Test
    @SmallTest
    public void testTabHasEmptyTitle() {
        MockTabModel mockTabModel = new MockTabModel(false, null);

        // Add a normal tab
        MockTab tab = (MockTab) mockTabModel.addTab(0);
        tab.setGurlOverrideForTesting(new GURL(TAB_URL + "0"));
        CriticalPersistedTabData.from(tab).setTitle(TAB_TITLE + "0");
        CriticalPersistedTabData.from(tab).setTimestampMillis(0);

        // Add an empty title tab
        tab = (MockTab) mockTabModel.addTab(1);
        tab.setGurlOverrideForTesting(new GURL(TAB_URL + "1"));
        CriticalPersistedTabData.from(tab).setTimestampMillis(1);
        CriticalPersistedTabData.from(tab).setTitle("");

        doReturn(mockTabModel).when(mTabModelSelector).getModel(false);
        AuxiliarySearchTabGroup tabGroup = mAuxiliarySearchProvider.getTabsSearchableDataProto();

        assertEquals(1, tabGroup.getTabCount());
        assertTrue(tabGroup.getTab(0).hasTitle());
        assertEquals(TAB_TITLE + "0", tabGroup.getTab(0).getTitle());
        assertTrue(tabGroup.getTab(0).hasUrl());
        assertEquals(TAB_URL + "0", tabGroup.getTab(0).getUrl());
    }

    @Test
    @SmallTest
    public void testTabHasNullUrl() {
        MockTabModel mockTabModel = new MockTabModel(false, null);

        // Add a normal tab
        MockTab tab = (MockTab) mockTabModel.addTab(0);
        tab.setGurlOverrideForTesting(new GURL(TAB_URL + "0"));
        CriticalPersistedTabData.from(tab).setTitle(TAB_TITLE + "0");
        CriticalPersistedTabData.from(tab).setTimestampMillis(0);

        // Add a null url tab
        tab = (MockTab) mockTabModel.addTab(1);
        tab.setGurlOverrideForTesting(null);
        CriticalPersistedTabData.from(tab).setTimestampMillis(1);
        CriticalPersistedTabData.from(tab).setTitle(TAB_TITLE + "0");

        doReturn(mockTabModel).when(mTabModelSelector).getModel(false);
        AuxiliarySearchTabGroup tabGroup = mAuxiliarySearchProvider.getTabsSearchableDataProto();

        assertEquals(1, tabGroup.getTabCount());
        assertTrue(tabGroup.getTab(0).hasTitle());
        assertEquals(TAB_TITLE + "0", tabGroup.getTab(0).getTitle());
        assertTrue(tabGroup.getTab(0).hasUrl());
        assertEquals(TAB_URL + "0", tabGroup.getTab(0).getUrl());
    }

    @Test
    @SmallTest
    public void testTabHasInvalidlUrl() {
        MockTabModel mockTabModel = new MockTabModel(false, null);

        // Add a normal tab
        MockTab tab = (MockTab) mockTabModel.addTab(0);
        tab.setGurlOverrideForTesting(new GURL(TAB_URL + "0"));
        CriticalPersistedTabData.from(tab).setTitle(TAB_TITLE + "0");
        CriticalPersistedTabData.from(tab).setTimestampMillis(0);

        // Add an invalid url tab
        tab = (MockTab) mockTabModel.addTab(1);
        tab.setGurlOverrideForTesting(new GURL("invalid"));
        CriticalPersistedTabData.from(tab).setTimestampMillis(1);
        CriticalPersistedTabData.from(tab).setTitle(TAB_TITLE + "0");

        doReturn(mockTabModel).when(mTabModelSelector).getModel(false);
        AuxiliarySearchTabGroup tabGroup = mAuxiliarySearchProvider.getTabsSearchableDataProto();

        assertEquals(1, tabGroup.getTabCount());
        assertTrue(tabGroup.getTab(0).hasTitle());
        assertEquals(TAB_TITLE + "0", tabGroup.getTab(0).getTitle());
        assertTrue(tabGroup.getTab(0).hasUrl());
        assertEquals(TAB_URL + "0", tabGroup.getTab(0).getUrl());
    }

    @Test
    @SmallTest
    public void testGetTabsSearchableDataProtoAsync() {
        MockTabModel mockTabModel = new MockTabModel(false, null);
        ArrayList<Tab> tabList = new ArrayList<>();
        // Create 200 tabs with different timestamps(from 0 to 199), and only the newest 100 tabs
        // should be returned from 'getTabsSearchableDataProtoAsync'.
        for (int i = 0; i < 200; i++) {
            MockTab tab = (MockTab) mockTabModel.addTab(i);
            tab.setGurlOverrideForTesting(new GURL(TAB_URL + Integer.toString(i)));
            CriticalPersistedTabData.from(tab).setTitle(TAB_TITLE + Integer.toString(i));
            CriticalPersistedTabData.from(tab).setTimestampMillis(i);
            if (i >= 100) {
                tabList.add(tab);
            }
        }

        Object[] tabObject = new Object[tabList.size()];
        tabList.toArray(tabObject);
        doReturn(mockTabModel).when(mTabModelSelector).getModel(false);
        doAnswer(invocation -> {
            invocation.<Callback<Object[]>>getArgument(2).onResult(tabObject);
            return null;
        })
                .when(mMockAuxiliarySearchBridgeJni)
                .getNonSensitiveTabs(eq(FAKE_NATIVE_PROVIDER), any(), any(Callback.class));

        mAuxiliarySearchProvider.getTabsSearchableDataProtoAsync(
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
                            assertTrue("Only the newest 100 tabs should be received",
                                    number >= 100 && number <= 199);
                            assertEquals(number, (int) tab.getLastAccessTimestamp());
                            returnedTabsNumbers.add(number);
                        }
                        assertEquals(returnedTabsNumbers.size(), 100);
                    }
                });
    }
}
