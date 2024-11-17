// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Self-documenting preference class for bookmarks. */
public class BookmarkUiPrefs {
    private static final @BookmarkRowDisplayPref int INITIAL_BOOKMARK_ROW_DISPLAY_PREF =
            BookmarkRowDisplayPref.VISUAL;
    private static final @BookmarkRowSortOrder int INITIAL_BOOKMARK_ROW_SORT_ORDER =
            BookmarkRowSortOrder.MANUAL;

    // These values are persisted to prefs/logs. Entries should not be renumbered and numeric
    // values should never be reused. Keep up-to-date with the
    // MobileBookmarkManagerBookmarkRowDisplayPref enum in tools/metrics/histograms/enums.xml.
    @IntDef({
        BookmarkRowDisplayPref.COMPACT,
        BookmarkRowDisplayPref.VISUAL,
        BookmarkRowDisplayPref.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkRowDisplayPref {
        int COMPACT = 0;
        int VISUAL = 1;
        int COUNT = 2;
    }

    // These values are persisted to prefs/logs. Entries should not be renumbered and numeric
    // values should never be reused. Keep up-to-date with the
    // MobileBookmarkManagerBookmarkRowSortOrder enum in tools/metrics/histograms/enums.xml.
    @IntDef({
        BookmarkRowSortOrder.CHRONOLOGICAL,
        BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL,
        BookmarkRowSortOrder.ALPHABETICAL,
        BookmarkRowSortOrder.REVERSE_ALPHABETICAL,
        BookmarkRowSortOrder.RECENTLY_USED,
        BookmarkRowSortOrder.MANUAL,
        BookmarkRowSortOrder.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkRowSortOrder {
        // Oldest -> newest
        int CHRONOLOGICAL = 0;
        int REVERSE_CHRONOLOGICAL = 1;
        int ALPHABETICAL = 2;
        int REVERSE_ALPHABETICAL = 3;
        int RECENTLY_USED = 4;
        int MANUAL = 5;
        int COUNT = 6;
    }

    /** Observer for changes to prefs. */
    public interface Observer {
        /** Called when the current {@link BookmarkRowDisplayPref} changes. */
        default void onBookmarkRowDisplayPrefChanged(@BookmarkRowDisplayPref int displayPref) {}

        // Called when the current {@link BookmarkRowSortOrder} changes. */
        default void onBookmarkRowSortOrderChanged(@BookmarkRowSortOrder int sortOrder) {}
    }

    private SharedPreferences.OnSharedPreferenceChangeListener mPrefsListener =
            new SharedPreferences.OnSharedPreferenceChangeListener() {
                @Override
                public void onSharedPreferenceChanged(SharedPreferences sharedPrefs, String key) {
                    if (key.equals(ChromePreferenceKeys.BOOKMARKS_VISUALS_PREF)) {
                        notifyObserversForDisplayPrefChange(
                                mPrefsManager.readInt(ChromePreferenceKeys.BOOKMARKS_VISUALS_PREF));
                    } else if (key.equals(ChromePreferenceKeys.BOOKMARKS_SORT_ORDER)) {
                        notifyObserversForSortOrderChange(
                                mPrefsManager.readInt(ChromePreferenceKeys.BOOKMARKS_SORT_ORDER));
                    }
                }
            };

    private final SharedPreferencesManager mPrefsManager;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * @param prefsManager Instance of {@link SharedPreferencesManager} to read/write from prefs.
     */
    // Suppress to observe SharedPreferences, which is discouraged; use another messaging channel
    // instead.
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    public BookmarkUiPrefs(SharedPreferencesManager prefsManager) {
        mPrefsManager = prefsManager;
        ContextUtils.getAppSharedPreferences()
                .registerOnSharedPreferenceChangeListener(mPrefsListener);
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
        return mPrefsManager.readInt(
                ChromePreferenceKeys.BOOKMARKS_VISUALS_PREF, INITIAL_BOOKMARK_ROW_DISPLAY_PREF);
    }

    /**
     * Sets the value for the bookmark row display pref.
     *
     * @param displayPref The pref value to be set.
     */
    public void setBookmarkRowDisplayPref(@BookmarkRowDisplayPref int displayPref) {
        BookmarkMetrics.reportBookmarkManagerDisplayPrefChanged(displayPref);
        mPrefsManager.writeInt(ChromePreferenceKeys.BOOKMARKS_VISUALS_PREF, displayPref);
    }

    void notifyObserversForDisplayPrefChange(@BookmarkRowDisplayPref int displayPref) {
        for (Observer obs : mObservers) obs.onBookmarkRowDisplayPrefChanged(displayPref);
    }

    /** Returns the order bookmark rows are displayed when not showing order in parent. */
    public @BookmarkRowSortOrder int getBookmarkRowSortOrder() {
        return mPrefsManager.readInt(
                ChromePreferenceKeys.BOOKMARKS_SORT_ORDER, INITIAL_BOOKMARK_ROW_SORT_ORDER);
    }

    /** Sets the order to sort bookmark rows. */
    public void setBookmarkRowSortOrder(@BookmarkRowSortOrder int sortOrder) {
        BookmarkMetrics.reportBookmarkManagerSortChanged(sortOrder);
        mPrefsManager.writeInt(ChromePreferenceKeys.BOOKMARKS_SORT_ORDER, sortOrder);
    }

    /**
     * Returns the text resource which is read aloud when a sort option is selected (for talkback).
     *
     * @param context The android context to get strings.
     * @param sortOrder The currently active sort order.
     * @return The string to be read aloud when the sort order is selected.
     */
    public String getSortOrderAccessibilityAnnouncementText(
            Context context, @BookmarkRowSortOrder int sortOrder) {
        int stringRes = 0;
        if (sortOrder == BookmarkRowSortOrder.CHRONOLOGICAL) {
            stringRes = R.string.sort_by_oldest_announcement;
        } else if (sortOrder == BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL) {
            stringRes = R.string.sort_by_newest_announcement;
        } else if (sortOrder == BookmarkRowSortOrder.ALPHABETICAL) {
            stringRes = R.string.sort_by_alpha_announcement;
        } else if (sortOrder == BookmarkRowSortOrder.REVERSE_ALPHABETICAL) {
            stringRes = R.string.sort_by_reverse_alpha_announcement;
        } else if (sortOrder == BookmarkRowSortOrder.RECENTLY_USED) {
            stringRes = R.string.sort_by_last_opened_announcement;
        } else if (sortOrder == BookmarkRowSortOrder.MANUAL) {
            stringRes = R.string.sort_by_manual_announcement;
        } else {
            assert false;
        }

        return context.getString(stringRes);
    }

    /**
     * Returns the text resource which is read aloud when a view option is selected (for talkback).
     *
     * @param context The android context to get strings.
     * @return The string to be read aloud when the view option is selected.
     */
    public String getViewOptionsAccessibilityAnnouncementText(
            Context context, @BookmarkRowDisplayPref int displayPref) {
        int stringRes = 0;
        if (displayPref == BookmarkRowDisplayPref.VISUAL) {
            stringRes = R.string.visual_view_announcement;
        } else if (displayPref == BookmarkRowDisplayPref.COMPACT) {
            stringRes = R.string.compact_view_announcement;
        } else {
            assert false;
        }

        return context.getString(stringRes);
    }

    void notifyObserversForSortOrderChange(@BookmarkRowSortOrder int sortOrder) {
        for (Observer obs : mObservers) obs.onBookmarkRowSortOrderChanged(sortOrder);
    }
}
