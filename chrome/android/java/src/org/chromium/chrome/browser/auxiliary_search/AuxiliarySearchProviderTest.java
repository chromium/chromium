// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.text.TextUtils;
import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.url.GURL;

import java.util.HashSet;
import java.util.List;

/**
 * Unit tests for {@link AuxiliarySearchProvider}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION})
public class AuxiliarySearchProviderTest {
    private static final String TAB_TITLE = "tab";
    private static final String TAB_URL = "https://tab.google.com/";
    private static final String BOOKMARK_TITLE = "bookmark";
    private static final String BOOKMARK_URL = "https://bookmark.google.com";
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
        mAuxiliarySearchProvider = new AuxiliarySearchProvider(mProfile, mTabModelSelector);
    }

    @Test
    @SmallTest
    public void testgetTabsSearchableData() throws InterruptedException {
        MockTabModel mockTabModel = new MockTabModel(false, null);
        for (int i = 0; i < 200; i++) {
            MockTab tab = (MockTab) mockTabModel.addTab(i);
            tab.setGurlOverrideForTesting(new GURL(TAB_URL + Integer.toString(i)));
            CriticalPersistedTabData.from(tab).setTitle(TAB_TITLE + Integer.toString(i));
        }

        doReturn(mockTabModel).when(mTabModelSelector).getModel(false);
        List<Pair<String, String>> tabsList = mAuxiliarySearchProvider.getTabsSearchableData();

        assertEquals(tabsList.size(), 100);
        HashSet<Integer> returnedTabsNumbers = new HashSet<Integer>();
        for (int i = 0; i < tabsList.size(); i++) {
            int number = Integer.valueOf(tabsList.get(i).second.substring(TAB_URL.length()));
            assertTrue(number >= 100 && number <= 199);
            returnedTabsNumbers.add(number);
        }
        assertEquals(returnedTabsNumbers.size(), 100);
    }

    @Test
    @SmallTest
    public void testgetBookmarksSearchableData() {
        var bookmark = AuxiliarySearchBookmarkGroup.Bookmark.newBuilder()
                               .setTitle(BOOKMARK_TITLE)
                               .setUrl(BOOKMARK_URL)
                               .build();
        var proto = AuxiliarySearchBookmarkGroup.newBuilder().addBookmark(bookmark).build();

        doReturn(proto.toByteArray())
                .when(mMockAuxiliarySearchBridgeJni)
                .getBookmarksSearchableData(FAKE_NATIVE_PROVIDER);

        List<Pair<String, String>> bookmarksList =
                mAuxiliarySearchProvider.getBookmarksSearchableData();

        assertEquals(bookmarksList.size(), 1);
        assertEquals(bookmarksList.get(0).first, BOOKMARK_TITLE);
        assertEquals(bookmarksList.get(0).second, BOOKMARK_URL);
    }

    @Test
    @SmallTest
    public void testgetBookmarksSearchableData_failureToParse() {
        // Return a random array which cannot been parsed to proto.
        doReturn(new byte[] {1, 2, 3})
                .when(mMockAuxiliarySearchBridgeJni)
                .getBookmarksSearchableData(FAKE_NATIVE_PROVIDER);

        List<Pair<String, String>> bookmarksList =
                mAuxiliarySearchProvider.getBookmarksSearchableData();

        bookmarksList = mAuxiliarySearchProvider.getBookmarksSearchableData();
        assertEquals(bookmarksList.size(), 0);
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
        List<Pair<String, String>> tabsList = mAuxiliarySearchProvider.getTabsSearchableData();

        assertEquals(1, tabsList.size());
        assertFalse(TextUtils.isEmpty(tabsList.get(0).first));
        assertEquals(TAB_TITLE + "0", tabsList.get(0).first);
        assertFalse(TextUtils.isEmpty(tabsList.get(0).second));
        assertEquals(TAB_URL + "0", tabsList.get(0).second);
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
        List<Pair<String, String>> tabsList = mAuxiliarySearchProvider.getTabsSearchableData();

        assertEquals(1, tabsList.size());
        assertFalse(TextUtils.isEmpty(tabsList.get(0).first));
        assertEquals(TAB_TITLE + "0", tabsList.get(0).first);
        assertFalse(TextUtils.isEmpty(tabsList.get(0).second));
        assertEquals(TAB_URL + "0", tabsList.get(0).second);
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
        List<Pair<String, String>> tabsList = mAuxiliarySearchProvider.getTabsSearchableData();

        assertEquals(1, tabsList.size());
        assertFalse(TextUtils.isEmpty(tabsList.get(0).first));
        assertEquals(TAB_TITLE + "0", tabsList.get(0).first);
        assertFalse(TextUtils.isEmpty(tabsList.get(0).second));
        assertEquals(TAB_URL + "0", tabsList.get(0).second);
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
        List<Pair<String, String>> tabsList = mAuxiliarySearchProvider.getTabsSearchableData();

        assertEquals(1, tabsList.size());
        assertFalse(TextUtils.isEmpty(tabsList.get(0).first));
        assertEquals(TAB_TITLE + "0", tabsList.get(0).first);
        assertFalse(TextUtils.isEmpty(tabsList.get(0).second));
        assertEquals(TAB_URL + "0", tabsList.get(0).second);
    }
}
