// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.DESKTOP_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.ROOT_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_D;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_E;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.Tracker;

import java.util.List;

/** Unit tests for {@link ImprovedBookmarkQueryHandler}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImprovedBookmarkQueryHandlerTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    Tracker mTracker;
    @Mock
    Profile mProfile;

    @Before
    public void setup() {
        Profile.setLastUsedProfileForTesting(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder() {
        BookmarkQueryHandler bookmarkQueryHandler =
                new ImprovedBookmarkQueryHandler(mBookmarkModel);
        List<BookmarkListEntry> result =
                bookmarkQueryHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        Assert.assertEquals(6, result.size());
        Assert.assertEquals(DESKTOP_BOOKMARK_ID, result.get(0).getBookmarkItem().getId());
        Assert.assertEquals(FOLDER_BOOKMARK_ID_A, result.get(1).getBookmarkItem().getId());
        Assert.assertEquals(READING_LIST_BOOKMARK_ID, result.get(2).getBookmarkItem().getId());
        Assert.assertEquals(URL_BOOKMARK_ID_A, result.get(3).getBookmarkItem().getId());
        Assert.assertEquals(URL_BOOKMARK_ID_D, result.get(4).getBookmarkItem().getId());
        Assert.assertEquals(URL_BOOKMARK_ID_E, result.get(5).getBookmarkItem().getId());
    }
}
