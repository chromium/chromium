// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.doReturn;

import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkListEntryTest {
    private static final int TITLE_RES = 1;
    private static final String TITLE = "title";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Resources mResources;

    @Before
    public void setUp() {
        doReturn(TITLE).when(mResources).getString(TITLE_RES);
    }

    @Test
    public void testGetTitle_BookmarkItem() {
        BookmarkId id = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkItem item =
                new BookmarkItem(
                        id,
                        TITLE,
                        new GURL("https://test.com"),
                        false,
                        id,
                        false,
                        false,
                        0,
                        false,
                        0,
                        false);
        BookmarkListEntry entry =
                BookmarkListEntry.createBookmarkEntry(
                        item, null, BookmarkUiPrefs.BookmarkRowDisplayPref.COMPACT);
        Assert.assertEquals(TITLE, entry.getTitle(mResources));
    }

    @Test
    public void testGetTitle_SectionHeader() {
        BookmarkListEntry entry = BookmarkListEntry.createSectionHeader(TITLE_RES, 0);
        Assert.assertEquals(TITLE, entry.getTitle(mResources));
    }

    @Test
    public void testGetTitle_NonBookmarkOrSectionHeader() {
        BookmarkListEntry entry = BookmarkListEntry.createBatchUploadCard();
        Assert.assertNull(entry.getTitle(mResources));
    }
}
