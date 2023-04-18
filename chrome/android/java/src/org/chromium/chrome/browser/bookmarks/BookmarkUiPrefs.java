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
    private static final @BookmarkRowDisplayPref int sInitialBookmarkRowDisplayPref =
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
        /** Called when the BookmarkRowDisplayPref changes. */
        void onBookmarkRowDisplayPrefChanged();
    }

    private final SharedPreferencesManager mPrefsManager;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * @param prefsManager Instance of SharedPreferencesManager to read/write from prefs.
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

    /** Returns how the bookmark rows should b edisplayed. */
    public @BookmarkRowDisplayPref int getBookmarkRowDisplayPref() {
        // Special cases for when the new visuals aren't enabled. We should either fallback to the
        // shopping visuals or the compact.
        if (!BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            return BookmarkFeatures.isLegacyBookmarksVisualRefreshEnabled()
                    ? BookmarkRowDisplayPref.VISUAL
                    : BookmarkRowDisplayPref.COMPACT;
        }

        if (mPrefsManager.contains(ChromePreferenceKeys.BOOKMARK_VISUALS_PREF)) {
            return mPrefsManager.readInt(ChromePreferenceKeys.BOOKMARK_VISUALS_PREF);
        } else {
            // The initial preference is controlled through a static member.
            setBookmarkRowDisplayPref(sInitialBookmarkRowDisplayPref);
            return sInitialBookmarkRowDisplayPref;
        }
    }

    /**
     * Sets the value for the bookmark row display pref.
     * @param pref The pref value to be set.
     */
    public void setBookmarkRowDisplayPref(@BookmarkRowDisplayPref int pref) {
        mPrefsManager.writeInt(ChromePreferenceKeys.BOOKMARK_VISUALS_PREF, pref);
        for (Observer obs : mObservers) obs.onBookmarkRowDisplayPrefChanged();
    }
}
