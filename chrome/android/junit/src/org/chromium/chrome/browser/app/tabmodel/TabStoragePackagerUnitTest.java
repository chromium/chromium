// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.WebContentsStateJni;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.tabs.TabStripCollection;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabStoragePackager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabStoragePackagerUnitTest {
    private static final int TAB_ID = 456;
    private static final String TITLE = "test_title";
    private static final String URL = "https://example.com";
    private static final long METADATA_PTR = 12345L;
    private static final long TIMESTAMP_MILLIS = 1000L;
    private static final int THEME_COLOR = 0xFF00FF;
    private static final long LAST_NAV_MILLIS = 2000L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private TabStoragePackager.Natives mTabStoragePackagerJni;
    @Mock private WebContentsState.Natives mWebContentsStateJni;

    private final ByteBuffer mByteBuffer = ByteBuffer.allocateDirect(16);
    private final List<JniRawPtr<?>> mTestRawPtrs = new ArrayList<>();

    @Before
    public void setUp() {
        TabStoragePackagerJni.setInstanceForTesting(mTabStoragePackagerJni);
        WebContentsStateJni.setInstanceForTesting(mWebContentsStateJni);

        doReturn(TAB_ID).when(mTab).getId();
        doReturn(TIMESTAMP_MILLIS).when(mTab).getTimestampMillis();
        doReturn(THEME_COLOR).when(mTab).getThemeColor();
        doReturn(LAST_NAV_MILLIS).when(mTab).getLastNavigationCommittedTimestampMillis();
        doReturn(false).when(mTab).getTabHasSensitiveContent();
        doReturn(new UserDataHost()).when(mTab).getUserDataHost();
        doReturn(mProfile).when(mTab).getProfile();
    }

    @After
    public void tearDown() {
        for (JniRawPtr<?> ptr : mTestRawPtrs) {
            ptr.release();
        }
        mTestRawPtrs.clear();
        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testFetchTabStorageMetadata_nullTab() {
        TabStoragePackager.fetchTabStorageMetadata(null, METADATA_PTR);
        verify(mTabStoragePackagerJni, never())
                .onTabStorageMetadataFetched(
                        anyLong(),
                        anyLong(),
                        anyInt(),
                        anyLong(),
                        anyBoolean(),
                        any(),
                        anyBoolean(),
                        any(),
                        anyInt());
    }

    @Test
    public void testFetchTabStorageMetadata_noPendingLoadParams_passesFalseAndNullBuffer() {
        doReturn(null).when(mTab).getPendingLoadParams();

        TabStoragePackager.fetchTabStorageMetadata(mTab, METADATA_PTR);
        verify(mTabStoragePackagerJni)
                .onTabStorageMetadataFetched(
                        eq(METADATA_PTR),
                        eq(TIMESTAMP_MILLIS),
                        eq(THEME_COLOR),
                        eq(LAST_NAV_MILLIS),
                        eq(false),
                        isNull(),
                        eq(false),
                        isNull(),
                        eq(WebContentsState.INVALID_BUFFER_VERSION));
    }

    @Test
    public void testFetchTabStorageMetadata_hasPendingLoadParams_singleNavigationState() {
        LoadUrlParams params = new LoadUrlParams(URL);
        doReturn(params).when(mTab).getPendingLoadParams();
        doReturn(TITLE).when(mTab).getTitle();
        doReturn(null).when(mTab).getWebContentsState();
        doReturn(null).when(mTab).getWebContents();

        doReturn(mByteBuffer)
                .when(mWebContentsStateJni)
                .createSingleNavigationStateAsByteBuffer(
                        eq(mProfile), eq(TITLE), eq(URL), any(), anyInt(), any());

        TabStoragePackager.fetchTabStorageMetadata(mTab, METADATA_PTR);
        verify(mTabStoragePackagerJni)
                .onTabStorageMetadataFetched(
                        eq(METADATA_PTR),
                        eq(TIMESTAMP_MILLIS),
                        eq(THEME_COLOR),
                        eq(LAST_NAV_MILLIS),
                        eq(false),
                        isNull(),
                        eq(true),
                        eq(mByteBuffer),
                        eq(WebContentsState.CONTENTS_STATE_CURRENT_VERSION));
    }

    @Test
    public void testFetchTabStorageMetadata_hasPendingLoadParams_emptyBuffer_passesNull() {
        LoadUrlParams params = new LoadUrlParams(URL);
        doReturn(params).when(mTab).getPendingLoadParams();
        doReturn(TITLE).when(mTab).getTitle();
        doReturn(null).when(mTab).getWebContentsState();
        doReturn(null).when(mTab).getWebContents();

        ByteBuffer emptyBuffer = ByteBuffer.allocateDirect(0);
        doReturn(emptyBuffer)
                .when(mWebContentsStateJni)
                .createSingleNavigationStateAsByteBuffer(
                        eq(mProfile), eq(TITLE), eq(URL), any(), anyInt(), any());

        TabStoragePackager.fetchTabStorageMetadata(mTab, METADATA_PTR);
        verify(mTabStoragePackagerJni)
                .onTabStorageMetadataFetched(
                        eq(METADATA_PTR),
                        eq(TIMESTAMP_MILLIS),
                        eq(THEME_COLOR),
                        eq(LAST_NAV_MILLIS),
                        eq(false),
                        isNull(),
                        eq(true),
                        isNull(),
                        eq(WebContentsState.INVALID_BUFFER_VERSION));
    }

    @Test
    public void
            testFetchTabStorageMetadata_liveWebContentsWithPendingLoad_appendsPendingNavigation() {
        WebContents webContents = mock(WebContents.class);
        doReturn(webContents).when(mTab).getWebContents();
        LoadUrlParams params = new LoadUrlParams(URL);
        doReturn(params).when(mTab).getPendingLoadParams();
        doReturn(TITLE).when(mTab).getTitle();
        doReturn(null).when(mTab).getWebContentsState();

        doReturn(mByteBuffer)
                .when(mWebContentsStateJni)
                .getContentsStateAsByteBuffer(eq(webContents));

        ByteBuffer appendedBuffer = ByteBuffer.allocateDirect(32);
        doReturn(appendedBuffer)
                .when(mWebContentsStateJni)
                .appendPendingNavigation(
                        eq(mProfile),
                        eq(mByteBuffer),
                        eq(WebContentsState.CONTENTS_STATE_CURRENT_VERSION),
                        anyBoolean(),
                        eq(TITLE),
                        any(),
                        any(),
                        anyInt(),
                        any());

        TabStoragePackager.fetchTabStorageMetadata(mTab, METADATA_PTR);
        verify(mTabStoragePackagerJni)
                .onTabStorageMetadataFetched(
                        eq(METADATA_PTR),
                        eq(TIMESTAMP_MILLIS),
                        eq(THEME_COLOR),
                        eq(LAST_NAV_MILLIS),
                        eq(false),
                        isNull(),
                        eq(true),
                        eq(appendedBuffer),
                        eq(WebContentsState.CONTENTS_STATE_CURRENT_VERSION));
    }

    @Test
    public void testGetWindowTag_windowScopedSelector_skipsOtherProfile() {
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        Profile otherProfile = mock(Profile.class);
        TabModelSelector otherSelector = mock(TabModelSelector.class);
        doReturn(otherProfile).when(otherSelector).getProfile(false);

        TabModelSelector matchingSelector = mock(TabModelSelector.class);
        doReturn(mProfile).when(matchingSelector).getProfile(false);

        JniPtr<TabStripCollection> collection = createTestCollectionPtr(101L);
        TabModel tabModel = createMockTabModel(collection, /* isOffTheRecord= */ false);
        doReturn(tabModel).when(matchingSelector).getTabModelForTabStripCollection(collection);

        doReturn(List.of(otherSelector, matchingSelector))
                .when(tabWindowManager)
                .getAllTabModelSelectors();
        doReturn(2).when(tabWindowManager).getWindowIdForSelector(matchingSelector);

        TabStoragePackager packager = new TabStoragePackager();
        assertEquals("2", packager.getWindowTag(mProfile, collection));
        verify(otherSelector, never()).getTabModelForTabStripCollection(any());
    }

    @Test
    public void testGetWindowTag_customTabSelector_skipsOtherProfile() {
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        Profile otherProfile = mock(Profile.class);
        TabModelSelector otherSelector = mock(TabModelSelector.class);
        doReturn(otherProfile).when(otherSelector).getProfile(false);

        TabModelSelector matchingSelector = mock(TabModelSelector.class);
        doReturn(mProfile).when(matchingSelector).getProfile(false);

        JniPtr<TabStripCollection> collection = createTestCollectionPtr(202L);
        TabModel tabModel = createMockTabModel(collection, /* isOffTheRecord= */ false);
        doReturn(tabModel).when(matchingSelector).getTabModelForTabStripCollection(collection);

        doReturn(List.of()).when(tabWindowManager).getAllTabModelSelectors();
        doReturn(List.of(otherSelector, matchingSelector))
                .when(tabWindowManager)
                .getCustomTabsTabModelSelectors();
        doReturn(20).when(tabWindowManager).getTaskIdForCustomTab(matchingSelector);

        TabStoragePackager packager = new TabStoragePackager();
        assertEquals("Custom_20", packager.getWindowTag(mProfile, collection));
        verify(otherSelector, never()).getTabModelForTabStripCollection(any());
    }

    @Test
    public void testGetWindowTag_otrProfileMatchesOriginalProfileSelector() {
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        Profile otrProfile = mock(Profile.class);
        doReturn(true).when(otrProfile).isOffTheRecord();
        doReturn(mProfile).when(otrProfile).getOriginalProfile();

        TabModelSelector matchingSelector = mock(TabModelSelector.class);
        doReturn(mProfile).when(matchingSelector).getProfile(false);

        JniPtr<TabStripCollection> collection = createTestCollectionPtr(303L);
        TabModel tabModel = createMockTabModel(collection, /* isOffTheRecord= */ true);
        doReturn(tabModel).when(matchingSelector).getTabModelForTabStripCollection(collection);

        doReturn(List.of(matchingSelector)).when(tabWindowManager).getAllTabModelSelectors();
        doReturn(3).when(tabWindowManager).getWindowIdForSelector(matchingSelector);

        TabStoragePackager packager = new TabStoragePackager();
        assertEquals("3", packager.getWindowTag(otrProfile, collection));
    }

    private JniRawPtr<TabStripCollection> createTestCollectionPtr(long fakePtr) {
        JniRawPtr<TabStripCollection> ptr = JniRawPtr.createForTesting(fakePtr);
        mTestRawPtrs.add(ptr);
        return ptr;
    }

    private TabModel createMockTabModel(
            JniPtr<TabStripCollection> collection, boolean isOffTheRecord) {
        TabModel tabModel = mock(TabModel.class);
        doReturn(collection).when(tabModel).getTabStripCollection();
        doReturn(isOffTheRecord).when(tabModel).isOffTheRecord();
        doReturn(ObservableSuppliers.alwaysNull()).when(tabModel).getCurrentTabSupplier();
        return tabModel;
    }
}
