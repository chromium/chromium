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

import java.util.Date;

/** Tests the use of the Searches URI as part of the Android provider public API. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.UNIT_TESTS)
public class ProviderSearchesUriTest {
    @Rule public ProviderTestRule mProviderTestRule = new ProviderTestRule();

    private Uri mSearchesUri;

    @Before
    public void setUp() {
        mSearchesUri =
                ChromeBrowserProviderImpl.getSearchesApiUri(ContextUtils.getApplicationContext());
    }

    private Uri addSearchTerm(String searchTerm, long searchTime) {
        ContentValues values = new ContentValues();
        values.put(SearchColumns.SEARCH, searchTerm);
        values.put(SearchColumns.DATE, searchTime);
        return mProviderTestRule.getContentResolver().insert(mSearchesUri, values);
    }

    @Test
    @SmallTest
    @Feature({"Android-ContentProvider"})
    public void testAddSearchTerm() {
        long searchTime = System.currentTimeMillis();
        String searchTerm = "chrome";
        Assert.assertNull(addSearchTerm(searchTerm, searchTime));
    }

    @Test
    @SmallTest
    @Feature({"Android-ContentProvider"})
    public void testUpdateSearchTerm() {
        ContentValues values = new ContentValues();
        values.put(SearchColumns.SEARCH, "chromium");
        values.put(SearchColumns.DATE, System.currentTimeMillis());

        Assert.assertEquals(
                0,
                mProviderTestRule.getContentResolver().update(Uri.parse(""), values, null, null));
    }

    @Test
    @SmallTest
    @Feature({"Android-ContentProvider"})
    public void testDeleteSearchTerm() {
        Assert.assertEquals(
                0, mProviderTestRule.getContentResolver().delete(Uri.parse(""), null, null));
    }

    @Test
    @SmallTest
    @Feature({"Android-ContentProvider"})
    public void testSearchesTable() {
        final int idIndex = 0;
        String insertSearch = "search_insert";
        String updateSearch = "search_update";

        // Test: insert
        ContentValues value = new ContentValues();
        long createDate = new Date().getTime();
        value.put(SearchColumns.SEARCH, insertSearch);
        value.put(SearchColumns.DATE, createDate);

        Cursor cursor =
                mProviderTestRule
                        .getContentResolver()
                        .query(
                                mSearchesUri,
                                ChromeBrowserProviderImpl.SEARCHES_PROJECTION,
                                SearchColumns.SEARCH + " = ?",
                                new String[] {insertSearch},
                                null);
        try {
            Assert.assertEquals(0, cursor.getCount());
        } finally {
            cursor.close();
        }
    }
}
