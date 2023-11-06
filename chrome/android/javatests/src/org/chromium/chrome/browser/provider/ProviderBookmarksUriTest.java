// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests the use of the Bookmark URI as part of the Android provider public API. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.UNIT_TESTS)
public class ProviderBookmarksUriTest {
    @Rule public ProviderTestRule mProviderTestRule = new ProviderTestRule();

    private static final byte[] FAVICON_DATA = {1, 2, 3};

    private Uri mBookmarksUri;

    @Before
    public void setUp() {
        mBookmarksUri =
                ChromeBrowserProviderImpl.getBookmarksApiUri(ContextUtils.getApplicationContext());
    }

    private Uri addBookmark(
            String url,
            String title,
            long lastVisitTime,
            long created,
            int visits,
            byte[] icon,
            int isBookmark) {
        ContentValues values = new ContentValues();
        values.put(BookmarkColumns.BOOKMARK, isBookmark);
        values.put(BookmarkColumns.DATE, lastVisitTime);
        values.put(BookmarkColumns.CREATED, created);
        values.put(BookmarkColumns.FAVICON, icon);
        values.put(BookmarkColumns.URL, url);
        values.put(BookmarkColumns.VISITS, visits);
        values.put(BookmarkColumns.TITLE, title);
        return mProviderTestRule.getContentResolver().insert(mBookmarksUri, values);
    }

    @Test
    @SmallTest
    @Feature({"Android-ContentProvider"})
    public void testAddBookmark() {
        final long lastUpdateTime = System.currentTimeMillis();
        final long createdTime = lastUpdateTime - 1000 * 60 * 60;
        final String url = "http://www.google.com/";
        final int visits = 2;
        final String title = "Google";
        ContentValues values = new ContentValues();
        values.put(BookmarkColumns.BOOKMARK, 0);
        values.put(BookmarkColumns.DATE, lastUpdateTime);
        values.put(BookmarkColumns.CREATED, createdTime);
        values.put(BookmarkColumns.FAVICON, FAVICON_DATA);
        values.put(BookmarkColumns.URL, url);
        values.put(BookmarkColumns.VISITS, visits);
        values.put(BookmarkColumns.TITLE, title);
        Assert.assertNull(mProviderTestRule.getContentResolver().insert(mBookmarksUri, values));
    }

    @Test
    @SmallTest
    @Feature({"Android-ContentProvider"})
    public void testQueryBookmark() {
        final long lastUpdateTime = System.currentTimeMillis();
        final String url = "http://www.google.com/";
        final int visits = 2;
        final int isBookmark = 1;

        String[] selectionArgs = {
            url, String.valueOf(lastUpdateTime), String.valueOf(visits), String.valueOf(isBookmark)
        };
        Cursor cursor =
                mProviderTestRule
                        .getContentResolver()
                        .query(
                                mBookmarksUri,
                                null,
                                BookmarkColumns.URL
                                        + " = ? AND "
                                        + BookmarkColumns.DATE
                                        + " = ? AND "
                                        + BookmarkColumns.VISITS
                                        + " = ? AND "
                                        + BookmarkColumns.BOOKMARK
                                        + " = ? AND "
                                        + BookmarkColumns.FAVICON
                                        + " IS NOT NULL",
                                selectionArgs,
                                null);
        try {
            Assert.assertEquals(0, cursor.getCount());
        } finally {
            cursor.close();
        }
    }

    @Test
    @SmallTest
    @Feature({"Android-ContentProvider"})
    public void testUpdateBookmark() {
        final long now = System.currentTimeMillis();
        final long lastUpdateTime[] = {now, now - 1000 * 60};
        final String url[] = {"http://www.google.com/", "http://mail.google.com/"};
        final int visits[] = {2, 20};
        final String title[] = {"Google", "Mail"};
        final int isBookmark[] = {1, 0};

        ContentValues values = new ContentValues();
        values.put(BookmarkColumns.BOOKMARK, isBookmark[1]);
        values.put(BookmarkColumns.DATE, lastUpdateTime[1]);
        values.put(BookmarkColumns.URL, url[1]);
        values.putNull(BookmarkColumns.FAVICON);
        values.put(BookmarkColumns.TITLE, title[1]);
        values.put(BookmarkColumns.VISITS, visits[1]);
        String[] selectionArgs = {String.valueOf(lastUpdateTime[0]), String.valueOf(isBookmark[0])};
        Assert.assertEquals(
                0,
                mProviderTestRule
                        .getContentResolver()
                        .update(
                                Uri.parse(""),
                                values,
                                BookmarkColumns.FAVICON
                                        + " IS NOT NULL AND "
                                        + BookmarkColumns.DATE
                                        + "= ? AND "
                                        + BookmarkColumns.BOOKMARK
                                        + " = ?",
                                selectionArgs));
    }

    @Test
    @SmallTest
    @Feature({"Android-ContentProvider"})
    public void testDeleteBookmark() {
        final long now = System.currentTimeMillis();
        final long lastUpdateTime[] = {now, now - 1000 * 60};
        final int isBookmark[] = {1, 0};

        String[] selectionArgs = {String.valueOf(lastUpdateTime[0]), String.valueOf(isBookmark[0])};
        Assert.assertEquals(
                0,
                mProviderTestRule
                        .getContentResolver()
                        .delete(
                                mBookmarksUri,
                                BookmarkColumns.FAVICON
                                        + " IS NOT NULL AND "
                                        + BookmarkColumns.DATE
                                        + "= ? AND "
                                        + BookmarkColumns.BOOKMARK
                                        + " = ?",
                                selectionArgs));
    }
}
