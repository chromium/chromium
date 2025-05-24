// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.content.Context;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link BookmarkUiPrefs}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkUiPrefsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkUiPrefs.Observer mObserver;

    private SharedPreferencesManager mSharedPreferencesManager;
    private BookmarkUiPrefs mBookmarkUiPrefs;

    @Before
    public void setUp() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mBookmarkUiPrefs = new BookmarkUiPrefs(mSharedPreferencesManager);
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.BOOKMARKS_VISUALS_PREF);
    }

    @Test
    public void initialBookmarkRowDisplayPref() {
        // Nothing has been written to shared prefs manager.
        Assert.assertEquals(
                BookmarkRowDisplayPref.VISUAL, mBookmarkUiPrefs.getBookmarkRowDisplayPref());
    }

    @Test
    public void returnsStoredPref() {
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);
        Assert.assertEquals(
                BookmarkRowDisplayPref.VISUAL, mBookmarkUiPrefs.getBookmarkRowDisplayPref());

        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
        Assert.assertEquals(
                BookmarkRowDisplayPref.COMPACT, mBookmarkUiPrefs.getBookmarkRowDisplayPref());
    }

    @Test
    public void setBookmarkRowDisplayPref() {
        mBookmarkUiPrefs.addObserver(mObserver);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
        verify(mObserver).onBookmarkRowDisplayPrefChanged(BookmarkRowDisplayPref.COMPACT);

        reset(mObserver);
        mBookmarkUiPrefs.removeObserver(mObserver);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
        verifyNoInteractions(mObserver);
    }

    @Test
    public void testSortOrder() {
        Assert.assertEquals(
                BookmarkRowSortOrder.MANUAL, mBookmarkUiPrefs.getBookmarkRowSortOrder());

        mBookmarkUiPrefs.addObserver(mObserver);
        mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.ALPHABETICAL);
        verify(mObserver).onBookmarkRowSortOrderChanged(BookmarkRowSortOrder.ALPHABETICAL);
        Assert.assertEquals(
                BookmarkRowSortOrder.ALPHABETICAL, mBookmarkUiPrefs.getBookmarkRowSortOrder());

        reset(mObserver);
        mBookmarkUiPrefs.removeObserver(mObserver);
        mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.CHRONOLOGICAL);
        verifyNoInteractions(mObserver);
        Assert.assertEquals(
                BookmarkRowSortOrder.CHRONOLOGICAL, mBookmarkUiPrefs.getBookmarkRowSortOrder());
    }

    @Test
    public void testRowDisplayPref_changesInBackground() {
        Assert.assertEquals(
                BookmarkRowDisplayPref.VISUAL, mBookmarkUiPrefs.getBookmarkRowDisplayPref());

        mBookmarkUiPrefs.addObserver(mObserver);
        mSharedPreferencesManager.writeInt(
                ChromePreferenceKeys.BOOKMARKS_VISUALS_PREF, BookmarkRowDisplayPref.COMPACT);
        verify(mObserver).onBookmarkRowDisplayPrefChanged(BookmarkRowDisplayPref.COMPACT);
    }

    @Test
    public void testSortOrderPref_changesInBackground() {
        Assert.assertEquals(
                BookmarkRowSortOrder.MANUAL, mBookmarkUiPrefs.getBookmarkRowSortOrder());

        mBookmarkUiPrefs.addObserver(mObserver);
        mSharedPreferencesManager.writeInt(
                ChromePreferenceKeys.BOOKMARKS_SORT_ORDER, BookmarkRowSortOrder.CHRONOLOGICAL);
        verify(mObserver).onBookmarkRowSortOrderChanged(BookmarkRowSortOrder.CHRONOLOGICAL);
    }

    @Test
    public void testSortOrderAccessibilityAnnouncementText() {
        Context context = ContextUtils.getApplicationContext();
        Assert.assertEquals(
                "Sorting by oldest",
                mBookmarkUiPrefs.getSortOrderAccessibilityAnnouncementText(
                        context, BookmarkRowSortOrder.CHRONOLOGICAL));
        Assert.assertEquals(
                "Sorting by newest",
                mBookmarkUiPrefs.getSortOrderAccessibilityAnnouncementText(
                        context, BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL));
        Assert.assertEquals(
                "Sorting from A to Z",
                mBookmarkUiPrefs.getSortOrderAccessibilityAnnouncementText(
                        context, BookmarkRowSortOrder.ALPHABETICAL));
        Assert.assertEquals(
                "Sorting from Z to A",
                mBookmarkUiPrefs.getSortOrderAccessibilityAnnouncementText(
                        context, BookmarkRowSortOrder.REVERSE_ALPHABETICAL));
        Assert.assertEquals(
                "Sorting by last opened",
                mBookmarkUiPrefs.getSortOrderAccessibilityAnnouncementText(
                        context, BookmarkRowSortOrder.RECENTLY_USED));
        Assert.assertEquals(
                "Sorting by manual order",
                mBookmarkUiPrefs.getSortOrderAccessibilityAnnouncementText(
                        context, BookmarkRowSortOrder.MANUAL));
    }

    @Test
    public void testViewOptionsAccessibilityAnnouncementText() {
        Context context = ContextUtils.getApplicationContext();
        Assert.assertEquals(
                "Showing visual view",
                mBookmarkUiPrefs.getViewOptionsAccessibilityAnnouncementText(
                        context, BookmarkRowDisplayPref.VISUAL));
        Assert.assertEquals(
                "Showing compact view",
                mBookmarkUiPrefs.getViewOptionsAccessibilityAnnouncementText(
                        context, BookmarkRowDisplayPref.COMPACT));
    }
}
