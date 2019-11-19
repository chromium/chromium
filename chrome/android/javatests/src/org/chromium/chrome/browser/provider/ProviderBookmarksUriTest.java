// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.Arrays;
import java.util.Date;

/**
 * Tests the use of the Bookmark URI as part of the Android provider public API.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ProviderBookmarksUriTest {
    @Rule
    public ProviderTestRule mProviderTestRule = new ProviderTestRule();

    private static final String TAG = "ProviderBookmarkUriTest";
    private static final byte[] FAVICON_DATA = { 1, 2, 3 };

    private Uri mBookmarksUri;

    @Before
    public void setUp() {
        mBookmarksUri = ChromeBrowserProvider.getBookmarksApiUri(mProviderTestRule.getActivity());
        mProviderTestRule.getContentResolver().delete(mBookmarksUri, null, null);
    }

    @After
    public void tearDown() {
        mProviderTestRule.getContentResolver().delete(mBookmarksUri, null, null);
    }

    private Uri addBookmark(String url, String title, long lastVisitTime, long created, int visits,
            byte[] icon, int isBookmark) {
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
    @MediumTest
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
        Uri uri = mProviderTestRule.getContentResolver().insert(mBookmarksUri, values);
        Cursor cursor = mProviderTestRule.getContentResolver().query(uri, null, null, null, null);
        try {
            Assert.assertEquals(1, cursor.getCount());
            Assert.assertTrue(cursor.moveToNext());
            int index = cursor.getColumnIndex(BookmarkColumns.BOOKMARK);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(0, cursor.getInt(index));
            index = cursor.getColumnIndex(BookmarkColumns.CREATED);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(createdTime, cursor.getLong(index));
            index = cursor.getColumnIndex(BookmarkColumns.DATE);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(lastUpdateTime, cursor.getLong(index));
            index = cursor.getColumnIndex(BookmarkColumns.FAVICON);
            Assert.assertTrue(-1 != index);
            Assert.assertTrue(byteArraysEqual(FAVICON_DATA, cursor.getBlob(index)));
            index = cursor.getColumnIndex(BookmarkColumns.URL);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(url, cursor.getString(index));
            index = cursor.getColumnIndex(BookmarkColumns.VISITS);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(visits, cursor.getInt(index));
        } finally {
            cursor.close();
        }
    }

    @Test
    @MediumTest
    @Feature({"Android-ContentProvider"})
    @RetryOnFailure
    public void testQueryBookmark() {
        final long now = System.currentTimeMillis();
        final long lastUpdateTime[] = { now, now - 1000 * 60 };
        final long createdTime[] = { now - 1000 * 60 * 60, now - 1000 * 60 * 60 * 60 };
        final String url[] = { "http://www.google.com/", "http://mail.google.com/" };
        final int visits[] = { 2, 20 };
        final String title[] = { "Google", "Mail" };
        final int isBookmark[] = { 1, 0 };
        Uri[] uris = new Uri[2];
        byte[][] icons = { FAVICON_DATA, null };
        for (int i = 0; i < uris.length; i++) {
            uris[i] = addBookmark(url[i], title[i], lastUpdateTime[i], createdTime[i], visits[i],
                    icons[i], isBookmark[i]);
            Assert.assertNotNull(uris[i]);
        }

        // Query the 1st row.
        String[] selectionArgs = { url[0], String.valueOf(lastUpdateTime[0]),
                String.valueOf(visits[0]), String.valueOf(isBookmark[0]) };
        Cursor cursor = mProviderTestRule.getContentResolver().query(mBookmarksUri, null,
                BookmarkColumns.URL + " = ? AND " + BookmarkColumns.DATE + " = ? AND "
                        + BookmarkColumns.VISITS + " = ? AND " + BookmarkColumns.BOOKMARK
                        + " = ? AND " + BookmarkColumns.FAVICON + " IS NOT NULL",
                selectionArgs, null);
        try {
            Assert.assertNotNull(cursor);
            Assert.assertEquals(1, cursor.getCount());
            Assert.assertTrue(cursor.moveToNext());
            int index = cursor.getColumnIndex(BookmarkColumns.BOOKMARK);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(isBookmark[0], cursor.getInt(index));
            index = cursor.getColumnIndex(BookmarkColumns.CREATED);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(createdTime[0], cursor.getLong(index));
            index = cursor.getColumnIndex(BookmarkColumns.DATE);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(lastUpdateTime[0], cursor.getLong(index));
            index = cursor.getColumnIndex(BookmarkColumns.FAVICON);
            Assert.assertTrue(-1 != index);
            Assert.assertTrue(byteArraysEqual(icons[0], cursor.getBlob(index)));
            index = cursor.getColumnIndex(BookmarkColumns.URL);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(url[0], cursor.getString(index));
            index = cursor.getColumnIndex(BookmarkColumns.VISITS);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(visits[0], cursor.getInt(index));
        } finally {
            cursor.close();
        }

        // Query the 2nd row.
        String[] selectionArgs2 = { url[1], String.valueOf(lastUpdateTime[1]),
                String.valueOf(visits[1]), String.valueOf(isBookmark[1]) };
        cursor = mProviderTestRule.getContentResolver().query(mBookmarksUri, null,
                BookmarkColumns.URL + " = ? AND " + BookmarkColumns.DATE + " = ? AND "
                        + BookmarkColumns.VISITS + " = ? AND " + BookmarkColumns.BOOKMARK
                        + " = ? AND " + BookmarkColumns.FAVICON + " IS NULL",
                selectionArgs2, null);
        try {
            Assert.assertEquals(1, cursor.getCount());
            Assert.assertTrue(cursor.moveToNext());
            int index = cursor.getColumnIndex(BookmarkColumns.BOOKMARK);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(isBookmark[1], cursor.getInt(index));
            index = cursor.getColumnIndex(BookmarkColumns.CREATED);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(createdTime[1], cursor.getLong(index));
            index = cursor.getColumnIndex(BookmarkColumns.DATE);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(lastUpdateTime[1], cursor.getLong(index));
            index = cursor.getColumnIndex(BookmarkColumns.FAVICON);
            Assert.assertTrue(-1 != index);
            Assert.assertTrue(byteArraysEqual(icons[1], cursor.getBlob(index)));
            index = cursor.getColumnIndex(BookmarkColumns.URL);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(url[1], cursor.getString(index));
            index = cursor.getColumnIndex(BookmarkColumns.VISITS);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(visits[1], cursor.getInt(index));
        } finally {
            cursor.close();
        }
    }

    @Test
    @MediumTest
    @Feature({"Android-ContentProvider"})
    @RetryOnFailure
    public void testUpdateBookmark() {
        final long now = System.currentTimeMillis();
        final long lastUpdateTime[] = { now, now - 1000 * 60 };
        final long createdTime[] = { now - 1000 * 60 * 60, now - 1000 * 60 * 60 * 60 };
        final String url[] = { "http://www.google.com/", "http://mail.google.com/" };
        final int visits[] = { 2, 20 };
        final String title[] = { "Google", "Mail" };
        final int isBookmark[] = { 1, 0 };

        byte[][] icons = { FAVICON_DATA, null };
        Uri uri = addBookmark(url[0], title[0], lastUpdateTime[0], createdTime[0], visits[0],
                icons[0], isBookmark[0]);
        Assert.assertNotNull(uri);

        ContentValues values = new ContentValues();
        values.put(BookmarkColumns.BOOKMARK, isBookmark[1]);
        values.put(BookmarkColumns.DATE, lastUpdateTime[1]);
        values.put(BookmarkColumns.URL, url[1]);
        values.putNull(BookmarkColumns.FAVICON);
        values.put(BookmarkColumns.TITLE, title[1]);
        values.put(BookmarkColumns.VISITS, visits[1]);
        String[] selectionArgs = { String.valueOf(lastUpdateTime[0]),
                String.valueOf(isBookmark[0]) };
        mProviderTestRule.getContentResolver().update(uri, values,
                BookmarkColumns.FAVICON + " IS NOT NULL AND " + BookmarkColumns.DATE + "= ? AND "
                        + BookmarkColumns.BOOKMARK + " = ?",
                selectionArgs);
        Cursor cursor = mProviderTestRule.getContentResolver().query(uri, null, null, null, null);
        try {
            Assert.assertEquals(1, cursor.getCount());
            Assert.assertTrue(cursor.moveToNext());
            int index = cursor.getColumnIndex(BookmarkColumns.BOOKMARK);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(isBookmark[1], cursor.getInt(index));
            index = cursor.getColumnIndex(BookmarkColumns.CREATED);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(createdTime[0], cursor.getLong(index));
            index = cursor.getColumnIndex(BookmarkColumns.DATE);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(lastUpdateTime[1], cursor.getLong(index));
            index = cursor.getColumnIndex(BookmarkColumns.FAVICON);
            Assert.assertTrue(-1 != index);
            Assert.assertTrue(byteArraysEqual(icons[1], cursor.getBlob(index)));
            index = cursor.getColumnIndex(BookmarkColumns.URL);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(url[1], cursor.getString(index));
            index = cursor.getColumnIndex(BookmarkColumns.VISITS);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(visits[1], cursor.getInt(index));
        } finally {
            cursor.close();
        }
    }

    @Test
    @MediumTest
    @Feature({"Android-ContentProvider"})
    @RetryOnFailure
    public void testDeleteBookmark() {
        final long now = System.currentTimeMillis();
        final long lastUpdateTime[] = { now, now - 1000 * 60 };
        final long createdTime[] = { now - 1000 * 60 * 60, now - 1000 * 60 * 60 * 60 };
        final String url[] = { "http://www.google.com/", "http://mail.google.com/" };
        final int visits[] = { 2, 20 };
        final String title[] = { "Google", "Mail" };
        final int isBookmark[] = { 1, 0 };
        Uri[] uris = new Uri[2];
        byte[][] icons = { FAVICON_DATA, null };
        for (int i = 0; i < uris.length; i++) {
            uris[i] = addBookmark(url[i], title[i], lastUpdateTime[i], createdTime[i], visits[i],
                    icons[i], isBookmark[i]);
            Assert.assertNotNull(uris[i]);
        }

        String[] selectionArgs = { String.valueOf(lastUpdateTime[0]),
                String.valueOf(isBookmark[0]) };
        mProviderTestRule.getContentResolver().delete(mBookmarksUri,
                BookmarkColumns.FAVICON + " IS NOT NULL AND " + BookmarkColumns.DATE + "= ? AND "
                        + BookmarkColumns.BOOKMARK + " = ?",
                selectionArgs);
        Cursor cursor =
                mProviderTestRule.getContentResolver().query(uris[0], null, null, null, null);
        try {
            Assert.assertEquals(0, cursor.getCount());
        } finally {
            cursor.close();
        }
        cursor = mProviderTestRule.getContentResolver().query(uris[1], null, null, null, null);
        try {
            Assert.assertEquals(1, cursor.getCount());
        } finally {
            cursor.close();
        }
        String[] selectionArgs1 = { String.valueOf(lastUpdateTime[1]),
                String.valueOf(isBookmark[1]) };
        mProviderTestRule.getContentResolver().delete(mBookmarksUri,
                BookmarkColumns.FAVICON + " IS NULL AND " + BookmarkColumns.DATE + "= ? AND "
                        + BookmarkColumns.BOOKMARK + " = ?",
                selectionArgs1);
        cursor = mProviderTestRule.getContentResolver().query(uris[1], null, null, null, null);
        try {
            Assert.assertEquals(0, cursor.getCount());
        } finally {
            cursor.close();
        }
    }

    /*
     * Copied from CTS test with minor adaptations.
     */
    @Test
    @MediumTest
    @Feature({"Android-ContentProvider"})
    public void testBookmarksTable() {
        final String[] bookmarksProjection = new String[] {
                BookmarkColumns.ID, BookmarkColumns.URL, BookmarkColumns.VISITS,
                BookmarkColumns.DATE, BookmarkColumns.CREATED, BookmarkColumns.BOOKMARK,
                BookmarkColumns.TITLE, BookmarkColumns.FAVICON };
        final int idIndex = 0;
        final int urlIndex = 1;
        final int visitsIndex = 2;
        final int dataIndex = 3;
        final int createdIndex = 4;
        final int bookmarkIndex = 5;
        final int titleIndex = 6;
        final int faviconIndex = 7;

        final String insertBookmarkTitle = "bookmark_insert";
        final String insertBookmarkUrl = "www.bookmark_insert.com";

        final String updateBookmarkTitle = "bookmark_update";
        final String updateBookmarkUrl = "www.bookmark_update.com";

        // Test: insert.
        ContentValues value = new ContentValues();
        long createDate = new Date().getTime();
        value.put(BookmarkColumns.TITLE, insertBookmarkTitle);
        value.put(BookmarkColumns.URL, insertBookmarkUrl);
        value.put(BookmarkColumns.VISITS, 0);
        value.put(BookmarkColumns.DATE, createDate);
        value.put(BookmarkColumns.CREATED, createDate);
        value.put(BookmarkColumns.BOOKMARK, 0);

        Uri insertUri = mProviderTestRule.getContentResolver().insert(mBookmarksUri, value);
        Cursor cursor = mProviderTestRule.getContentResolver().query(mBookmarksUri,
                bookmarksProjection, BookmarkColumns.TITLE + " = ?",
                new String[] {insertBookmarkTitle}, BookmarkColumns.DATE);
        int id = -1;
        try {
            Assert.assertTrue(cursor.moveToNext());
            Assert.assertEquals(insertBookmarkTitle, cursor.getString(titleIndex));
            Assert.assertEquals(insertBookmarkUrl, cursor.getString(urlIndex));
            Assert.assertEquals(0, cursor.getInt(visitsIndex));
            Assert.assertEquals(createDate, cursor.getLong(dataIndex));
            Assert.assertEquals(createDate, cursor.getLong(createdIndex));
            Assert.assertEquals(0, cursor.getInt(bookmarkIndex));
            // TODO(michaelbai): according to the test this should be null instead of an empty
            // byte[]. BUG 6288508 assertTrue(cursor.isNull(FAVICON_INDEX));
            id = cursor.getInt(idIndex);
        } finally {
            cursor.close();
        }

        // Test: update.
        value.clear();
        long updateDate = new Date().getTime();
        value.put(BookmarkColumns.TITLE, updateBookmarkTitle);
        value.put(BookmarkColumns.URL, updateBookmarkUrl);
        value.put(BookmarkColumns.VISITS, 1);
        value.put(BookmarkColumns.DATE, updateDate);

        mProviderTestRule.getContentResolver().update(mBookmarksUri, value,
                BookmarkColumns.TITLE + " = ?", new String[] {insertBookmarkTitle});
        cursor = mProviderTestRule.getContentResolver().query(
                mBookmarksUri, bookmarksProjection, BookmarkColumns.ID + " = " + id, null, null);
        try {
            Assert.assertTrue(cursor.moveToNext());
            Assert.assertEquals(updateBookmarkTitle, cursor.getString(titleIndex));
            Assert.assertEquals(updateBookmarkUrl, cursor.getString(urlIndex));
            Assert.assertEquals(1, cursor.getInt(visitsIndex));
            Assert.assertEquals(updateDate, cursor.getLong(dataIndex));
            Assert.assertEquals(createDate, cursor.getLong(createdIndex));
            Assert.assertEquals(0, cursor.getInt(bookmarkIndex));
            // TODO(michaelbai): according to the test this should be null instead of an empty
            // byte[]. BUG 6288508 assertTrue(cursor.isNull(FAVICON_INDEX));
            Assert.assertEquals(id, cursor.getInt(idIndex));
        } finally {
            cursor.close();
        }

        // Test: delete.
        mProviderTestRule.getContentResolver().delete(insertUri, null, null);
        cursor = mProviderTestRule.getContentResolver().query(
                mBookmarksUri, bookmarksProjection, BookmarkColumns.ID + " = " + id, null, null);
        try {
            Assert.assertEquals(0, cursor.getCount());
        } finally {
            cursor.close();
        }
    }

    /**
     * Checks if two byte arrays are equal. Used to compare icons.
     * @return True if equal, false otherwise.
     */
    private static boolean byteArraysEqual(byte[] byte1, byte[] byte2) {
        if (byte1 == null && byte2 != null) {
            return byte2.length == 0;
        }
        if (byte2 == null && byte1 != null) {
            return byte1.length == 0;
        }
        return Arrays.equals(byte1, byte2);
    }
}
