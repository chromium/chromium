// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.DESKTOP_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.OTHER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.PARTNER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.ROOT_BOOKMARK_ID;

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
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.util.browser.Features;

/** Unit tests for {@link BookmarkUtils}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkUtilsTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    private BookmarkModel mBookmarkModel;

    @Before
    public void setup() {
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);
    }

    @Test
    public void testCanAddFolderWhileViewingParent() {
        assertFalse(BookmarkUtils.canAddFolderWhileViewingParent(mBookmarkModel, ROOT_BOOKMARK_ID));
        assertTrue(
                BookmarkUtils.canAddFolderWhileViewingParent(mBookmarkModel, DESKTOP_BOOKMARK_ID));
        assertTrue(
                BookmarkUtils.canAddFolderWhileViewingParent(mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertFalse(BookmarkUtils.canAddFolderWhileViewingParent(
                mBookmarkModel, READING_LIST_BOOKMARK_ID));
        assertFalse(
                BookmarkUtils.canAddFolderWhileViewingParent(mBookmarkModel, PARTNER_BOOKMARK_ID));
    }

    @Test
    public void testCanAddBookmarkWhileViewingParent() {
        assertFalse(
                BookmarkUtils.canAddBookmarkWhileViewingParent(mBookmarkModel, ROOT_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddBookmarkWhileViewingParent(
                mBookmarkModel, DESKTOP_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddBookmarkWhileViewingParent(
                mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertTrue(BookmarkUtils.canAddBookmarkWhileViewingParent(
                mBookmarkModel, READING_LIST_BOOKMARK_ID));
        assertFalse(BookmarkUtils.canAddBookmarkWhileViewingParent(
                mBookmarkModel, PARTNER_BOOKMARK_ID));
    }

    @Test
    public void testGetParentFolderForViewing() {
        assertEquals(MOBILE_BOOKMARK_ID,
                BookmarkUtils.getParentFolderForViewing(mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertEquals(ROOT_BOOKMARK_ID,
                BookmarkUtils.getParentFolderForViewing(mBookmarkModel, OTHER_BOOKMARK_ID));
    }
}
