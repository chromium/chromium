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

import java.util.Date;

/**
 * Tests the use of the Searches URI as part of the Android provider public API.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ProviderSearchesUriTest {
    @Rule
    public ProviderTestRule mProviderTestRule = new ProviderTestRule();

    private Uri mSearchesUri;

    @Before
    public void setUp() {
        mSearchesUri = ChromeBrowserProvider.getSearchesApiUri(mProviderTestRule.getActivity());
        mProviderTestRule.getContentResolver().delete(mSearchesUri, null, null);
    }

    @After
    public void tearDown() {
        mProviderTestRule.getContentResolver().delete(mSearchesUri, null, null);
    }

    private Uri addSearchTerm(String searchTerm, long searchTime) {
        ContentValues values = new ContentValues();
        values.put(SearchColumns.SEARCH, searchTerm);
        values.put(SearchColumns.DATE, searchTime);
        return mProviderTestRule.getContentResolver().insert(mSearchesUri, values);
    }

    @Test
    @MediumTest
    @Feature({"Android-ContentProvider"})
    @RetryOnFailure
    public void testAddSearchTerm() {
        long searchTime = System.currentTimeMillis();
        String searchTerm = "chrome";
        Uri uri = addSearchTerm(searchTerm, searchTime);
        Assert.assertNotNull(uri);
        String[] selectionArgs = { searchTerm, String.valueOf(searchTime) };
        Cursor cursor = mProviderTestRule.getContentResolver().query(uri, null,
                SearchColumns.SEARCH + "=? AND " + SearchColumns.DATE + " = ? ", selectionArgs,
                null);
        try {
            Assert.assertEquals(1, cursor.getCount());
            Assert.assertTrue(cursor.moveToNext());
            int index = cursor.getColumnIndex(SearchColumns.SEARCH);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(searchTerm, cursor.getString(index));
            index = cursor.getColumnIndex(SearchColumns.DATE);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(searchTime, cursor.getLong(index));
        } finally {
            cursor.close();
        }
    }

    @Test
    @MediumTest
    @Feature({"Android-ContentProvider"})
    public void testUpdateSearchTerm() {
        long[] searchTime = { System.currentTimeMillis(), System.currentTimeMillis() - 1000 };
        String[] searchTerm = { "chrome", "chromium" };
        Uri uri = addSearchTerm(searchTerm[0], searchTime[0]);
        ContentValues values = new ContentValues();
        values.put(SearchColumns.SEARCH, searchTerm[1]);
        values.put(SearchColumns.DATE, searchTime[1]);
        mProviderTestRule.getContentResolver().update(uri, values, null, null);
        String[] selectionArgs = { searchTerm[0] };
        Cursor cursor = mProviderTestRule.getContentResolver().query(
                mSearchesUri, null, SearchColumns.SEARCH + "=?", selectionArgs, null);
        try {
            Assert.assertEquals(0, cursor.getCount());
        } finally {
            cursor.close();
        }
        String[] selectionArgs1 = { searchTerm[1] };
        cursor = mProviderTestRule.getContentResolver().query(
                mSearchesUri, null, SearchColumns.SEARCH + "=?", selectionArgs1, null);
        try {
            Assert.assertEquals(1, cursor.getCount());
            Assert.assertTrue(cursor.moveToNext());
            int index = cursor.getColumnIndex(SearchColumns.SEARCH);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(searchTerm[1], cursor.getString(index));
            index = cursor.getColumnIndex(SearchColumns.DATE);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(searchTime[1], cursor.getLong(index));
        } finally {
            cursor.close();
        }
    }

    @Test
    @MediumTest
    @Feature({"Android-ContentProvider"})
    @RetryOnFailure
    public void testDeleteSearchTerm() {
        long[] searchTime = { System.currentTimeMillis(), System.currentTimeMillis() - 1000 };
        String[] searchTerm = {"chrome", "chromium"};
        Uri uri[] = new Uri[2];
        for (int i = 0; i < uri.length; i++) {
            uri[i] = addSearchTerm(searchTerm[i], searchTime[i]);
        }
        mProviderTestRule.getContentResolver().delete(uri[0], null, null);
        String[] selectionArgs = { searchTerm[0] };
        Cursor cursor = mProviderTestRule.getContentResolver().query(
                mSearchesUri, null, SearchColumns.SEARCH + "=?", selectionArgs, null);
        try {
            Assert.assertEquals(0, cursor.getCount());
        } finally {
            cursor.close();
        }
        String[] selectionArgs1 = {searchTerm[1]};
        cursor = mProviderTestRule.getContentResolver().query(
                mSearchesUri, null, SearchColumns.SEARCH + "=?", selectionArgs1, null);
        try {
            Assert.assertNotNull(cursor);
            Assert.assertEquals(1, cursor.getCount());
            Assert.assertTrue(cursor.moveToNext());
            int index = cursor.getColumnIndex(SearchColumns.SEARCH);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(searchTerm[1], cursor.getString(index));
            index = cursor.getColumnIndex(SearchColumns.DATE);
            Assert.assertTrue(-1 != index);
            Assert.assertEquals(searchTime[1], cursor.getLong(index));
            mProviderTestRule.getContentResolver().delete(uri[1], null, null);
        } finally {
            cursor.close();
        }
        cursor = mProviderTestRule.getContentResolver().query(uri[1], null, null, null, null);
        try {
            Assert.assertEquals(0, cursor.getCount());
        } finally {
            cursor.close();
        }
    }

    // Copied from CTS test with minor adaptations.
    @Test
    @MediumTest
    @Feature({"Android-ContentProvider"})
    @RetryOnFailure
    public void testSearchesTable() {
        final int idIndex = 0;
        String insertSearch = "search_insert";
        String updateSearch = "search_update";

        // Test: insert
        ContentValues value = new ContentValues();
        long createDate = new Date().getTime();
        value.put(SearchColumns.SEARCH, insertSearch);
        value.put(SearchColumns.DATE, createDate);

        Uri insertUri = mProviderTestRule.getContentResolver().insert(mSearchesUri, value);
        Cursor cursor = mProviderTestRule.getContentResolver().query(mSearchesUri,
                ChromeBrowserProvider.SEARCHES_PROJECTION, SearchColumns.SEARCH + " = ?",
                new String[] {insertSearch}, null);
        int id;
        try {
            Assert.assertTrue(cursor.moveToNext());
            Assert.assertEquals(insertSearch,
                    cursor.getString(ChromeBrowserProvider.SEARCHES_PROJECTION_SEARCH_INDEX));
            Assert.assertEquals(createDate,
                    cursor.getLong(ChromeBrowserProvider.SEARCHES_PROJECTION_DATE_INDEX));
            id = cursor.getInt(idIndex);
        } finally {
            cursor.close();
        }

        // Test: update
        value.clear();
        long updateDate = new Date().getTime();
        value.put(SearchColumns.SEARCH, updateSearch);
        value.put(SearchColumns.DATE, updateDate);

        mProviderTestRule.getContentResolver().update(
                mSearchesUri, value, SearchColumns.ID + " = " + id, null);
        cursor = mProviderTestRule.getContentResolver().query(mSearchesUri,
                ChromeBrowserProvider.SEARCHES_PROJECTION, SearchColumns.ID + " = " + id, null,
                null);
        try {
            Assert.assertTrue(cursor.moveToNext());
            Assert.assertEquals(updateSearch,
                    cursor.getString(ChromeBrowserProvider.SEARCHES_PROJECTION_SEARCH_INDEX));
            Assert.assertEquals(updateDate,
                    cursor.getLong(ChromeBrowserProvider.SEARCHES_PROJECTION_DATE_INDEX));
            Assert.assertEquals(id, cursor.getInt(idIndex));
        } finally {
            cursor.close();
        }
        // Test: delete
        mProviderTestRule.getContentResolver().delete(insertUri, null, null);
        cursor = mProviderTestRule.getContentResolver().query(mSearchesUri,
                ChromeBrowserProvider.SEARCHES_PROJECTION, SearchColumns.ID + " = " + id, null,
                null);
        try {
            Assert.assertEquals(0, cursor.getCount());
        } finally {
            cursor.close();
        }
    }
}
