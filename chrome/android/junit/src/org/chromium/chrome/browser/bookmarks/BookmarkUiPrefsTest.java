// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.util.browser.Features;

/** Unit tests for {@link BookmarkUiPrefs}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS})
public class BookmarkUiPrefsTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    BookmarkUiPrefs.Observer mObserver;

    SharedPreferencesManager mSharedPreferencesManager;
    BookmarkUiPrefs mBookmarkUiPrefs;

    @Before
    public void setUp() {
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
        mBookmarkUiPrefs = new BookmarkUiPrefs(mSharedPreferencesManager);
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.BOOKMARK_VISUALS_PREF);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS})
    @Features.EnableFeatures({ChromeFeatureList.BOOKMARKS_REFRESH})
    public void legacyVisualFlags() {
        ShoppingFeatures.setShoppingListEligibleForTesting(true);

        Assert.assertEquals(
                BookmarkRowDisplayPref.VISUAL, BookmarkUiPrefs.getBookmarkRowDisplayPref());

        ShoppingFeatures.setShoppingListEligibleForTesting(false);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS})
    @Features.EnableFeatures({ChromeFeatureList.BOOKMARKS_REFRESH})
    public void legacyVisualFlags_noShopping() {
        ShoppingFeatures.setShoppingListEligibleForTesting(false);

        // Nothing has been written to shared prefs manager.
        Assert.assertEquals(
                BookmarkRowDisplayPref.COMPACT, BookmarkUiPrefs.getBookmarkRowDisplayPref());
    }

    @Test
    public void initialBookmarkRowDisplayPref() {
        // Nothing has been written to shared prefs manager.
        Assert.assertEquals(
                BookmarkRowDisplayPref.COMPACT, BookmarkUiPrefs.getBookmarkRowDisplayPref());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS})
    public void returnsStoredPref() {
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);
        Assert.assertEquals(
                BookmarkRowDisplayPref.VISUAL, BookmarkUiPrefs.getBookmarkRowDisplayPref());

        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
        Assert.assertEquals(
                BookmarkRowDisplayPref.COMPACT, BookmarkUiPrefs.getBookmarkRowDisplayPref());
    }

    @Test
    public void setBookmarkRowDisplayPref() {
        BookmarkUiPrefs.Observer obs = Mockito.mock(BookmarkUiPrefs.Observer.class);

        mBookmarkUiPrefs.addObserver(obs);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
        verify(obs).onBookmarkRowDisplayPrefChanged();

        mBookmarkUiPrefs.removeObserver(obs);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
        // The observer method shouldn't have been called again.
        verify(obs, times(1)).onBookmarkRowDisplayPrefChanged();
    }
}
