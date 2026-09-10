// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.url.GURL;

/** Unit tests for {@link ColdBackgroundTab}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ColdBackgroundTabTest {
    private static final @TabId int ORIGINAL_TAB_ID = 101;
    private static final @TabId int PLACEHOLDER_TAB_ID = 202;
    private static final int DESTINATION_INDEX = 3;

    public final @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock BackgroundTabPool mBackgroundTabPool;
    private @Mock TabModel mTabModel;
    private @Mock TabCreator mTabCreator;
    private @Mock TabRemover mTabRemover;
    private @Mock Tab mTab;
    private @Mock Tab mPlaceholderTab;
    private @Mock WebContentsState mBackgroundContentsState;
    private @Mock WebContentsState mPlaceholderContentsState;

    private TabState mBackgroundTabState;

    @Before
    public void setUp() {
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.getTabById(PLACEHOLDER_TAB_ID)).thenReturn(null);

        mBackgroundTabState = new TabState();
        mBackgroundTabState.contentsState = mBackgroundContentsState;
        mBackgroundTabState.url = new GURL("https://www.google.com");
        mBackgroundTabState.timestampMillis = 12345L;
        mBackgroundTabState.lastNavigationCommittedTimestampMillis = 67890L;
        mBackgroundTabState.userAgent = 2;
        mBackgroundTabState.themeColor = 0xFF00FF;
        mBackgroundTabState.tabHasSensitiveContent = true;
    }

    @Test
    public void testGetters() {
        ColdBackgroundTab tab =
                new ColdBackgroundTab(
                        mBackgroundTabPool,
                        ORIGINAL_TAB_ID,
                        mBackgroundTabState,
                        PLACEHOLDER_TAB_ID);

        assertEquals(ORIGINAL_TAB_ID, tab.getOriginalTabId());
        assertEquals(PLACEHOLDER_TAB_ID, tab.getPlaceholderTabId());
    }

    @Test
    public void testAttachTab_createsFrozenTabWithOriginalTabIdAndDestroysPlaceholderState() {
        Token placeholderGroupToken = new Token(3L, 4L);
        TabState placeholderState = new TabState();
        placeholderState.contentsState = mPlaceholderContentsState;
        placeholderState.tabGroupId = placeholderGroupToken;
        placeholderState.rootId = 555;
        placeholderState.isPinned = true;

        ColdBackgroundTab tab =
                new ColdBackgroundTab(
                        mBackgroundTabPool,
                        ORIGINAL_TAB_ID,
                        mBackgroundTabState,
                        PLACEHOLDER_TAB_ID);

        when(mTabCreator.createFrozenTab(any(), eq(ORIGINAL_TAB_ID), eq(DESTINATION_INDEX)))
                .thenReturn(mTab);

        Tab attachedTab = tab.attachTab(mTabModel, DESTINATION_INDEX, placeholderState);

        assertEquals(mTab, attachedTab);
        assertEquals(placeholderGroupToken, mBackgroundTabState.tabGroupId);
        assertEquals(555, mBackgroundTabState.rootId);
        assertTrue(mBackgroundTabState.isPinned);
        verify(mBackgroundTabPool).removeTabById(ORIGINAL_TAB_ID);
        verify(mTabCreator)
                .createFrozenTab(
                        eq(mBackgroundTabState), eq(ORIGINAL_TAB_ID), eq(DESTINATION_INDEX));
        verify(mPlaceholderContentsState).destroy();
    }

    @Test
    public void testAttachTab_nullPlaceholderState() {
        ColdBackgroundTab tab =
                new ColdBackgroundTab(
                        mBackgroundTabPool,
                        ORIGINAL_TAB_ID,
                        mBackgroundTabState,
                        PLACEHOLDER_TAB_ID);

        when(mTabCreator.createFrozenTab(any(), eq(ORIGINAL_TAB_ID), eq(DESTINATION_INDEX)))
                .thenReturn(mTab);

        Tab attachedTab =
                tab.attachTab(mTabModel, DESTINATION_INDEX, /* placeholderTabState= */ null);

        assertEquals(mTab, attachedTab);
        verify(mBackgroundTabPool).removeTabById(ORIGINAL_TAB_ID);
        verify(mTabCreator)
                .createFrozenTab(
                        eq(mBackgroundTabState), eq(ORIGINAL_TAB_ID), eq(DESTINATION_INDEX));
    }

    @Test
    public void testDestroy_destroysWebContentsState() {
        ColdBackgroundTab tab =
                new ColdBackgroundTab(
                        mBackgroundTabPool,
                        ORIGINAL_TAB_ID,
                        mBackgroundTabState,
                        PLACEHOLDER_TAB_ID);

        tab.destroy();

        verify(mBackgroundContentsState).destroy();
        assertNull(mBackgroundTabState.contentsState);
    }

    @Test
    public void testAttachTab_transfersZeroRootId() {
        TabState placeholderState = new TabState();
        placeholderState.rootId = 0;

        ColdBackgroundTab tab =
                new ColdBackgroundTab(
                        mBackgroundTabPool,
                        ORIGINAL_TAB_ID,
                        mBackgroundTabState,
                        PLACEHOLDER_TAB_ID);

        when(mTabCreator.createFrozenTab(any(), eq(ORIGINAL_TAB_ID), eq(DESTINATION_INDEX)))
                .thenReturn(mTab);

        tab.attachTab(mTabModel, DESTINATION_INDEX, placeholderState);

        assertEquals(0, mBackgroundTabState.rootId);
    }

    @Test
    public void testAttachTab_withPlaceholderTabInModel_removesPlaceholder() {
        when(mTabModel.getTabById(PLACEHOLDER_TAB_ID)).thenReturn(mPlaceholderTab);

        ColdBackgroundTab tab =
                new ColdBackgroundTab(
                        mBackgroundTabPool,
                        ORIGINAL_TAB_ID,
                        mBackgroundTabState,
                        PLACEHOLDER_TAB_ID);

        when(mTabCreator.createFrozenTab(any(), eq(ORIGINAL_TAB_ID), eq(DESTINATION_INDEX)))
                .thenReturn(mTab);

        tab.attachTab(mTabModel, DESTINATION_INDEX, /* placeholderTabState= */ null);

        verify(mTabRemover).removeTab(mPlaceholderTab, /* allowDialog= */ false);
        verify(mPlaceholderTab).destroy();
    }
}
