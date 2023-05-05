// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Self-documenting preference class for bookmarks.
 */
public class BookmarkUiPrefs {
    private static final @BookmarkRowDisplayPref int INITIAL_BOOKMARK_ROW_DISPLAY_PREF =
            BookmarkRowDisplayPref.COMPACT;

    // This is persisted to preferences, entries shouldn't be reordered or removed.
    @IntDef({BookmarkRowDisplayPref.COMPACT, BookmarkRowDisplayPref.VISUAL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkRowDisplayPref {
        int COMPACT = 0;
        int VISUAL = 1;
    }

    /** Observer for changes to prefs. */
    public interface Observer {
        /** Called when the current {@link BookmarkRowDisplayPref} changes. */
        void onBookmarkRowDisplayPrefChanged(@BookmarkRowDisplayPref int displayPref);
    }

    private final SharedPreferencesManager mPrefsManager;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * @param prefsManager Instance of {@link SharedPreferencesManager} to read/write from prefs.
     */
    public BookmarkUiPrefs(SharedPreferencesManager prefsManager) {
        mPrefsManager = prefsManager;
    }

    /** Add the given observer to the list. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Remove the given observer from the list. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /** Returns how the bookmark rows should be displayed, doesn't write anything to prefs. */
    public @BookmarkRowDisplayPref int getBookmarkRowDisplayPref() {
        // Special cases for when the new visuals aren't enabled. We should either fallback to the
        // shopping visuals or the compact.
        if (!BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            return getDisplayPrefForLegacy();
        }

        return mPrefsManager.readInt(
                ChromePreferenceKeys.BOOKMARKS_VISUALS_PREF, INITIAL_BOOKMARK_ROW_DISPLAY_PREF);
    }

    /**
     * Sets the value for the bookmark row display pref.
     * @param displayPref The pref value to be set.
     */
    public void setBookmarkRowDisplayPref(@BookmarkRowDisplayPref int displayPref) {
        mPrefsManager.writeInt(ChromePreferenceKeys.BOOKMARKS_VISUALS_PREF, displayPref);
        for (Observer obs : mObservers) obs.onBookmarkRowDisplayPrefChanged(displayPref);
    }

    /**
     * Some places use {@link BookmarkRowDisplayPref} even for legacy handling. This converts to the
     * new display pref from feature flags.
     */
    public static @BookmarkRowDisplayPref int getDisplayPrefForLegacy() {
        assert !BookmarkFeatures.isAndroidImprovedBookmarksEnabled();
        return BookmarkFeatures.isLegacyBookmarksVisualRefreshEnabled()
                ? BookmarkRowDisplayPref.VISUAL
                : BookmarkRowDisplayPref.COMPACT;
    }
}
