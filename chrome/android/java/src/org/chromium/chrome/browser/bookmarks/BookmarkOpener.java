// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.text.format.DateUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.ui.base.PageTransition;

import java.util.ArrayList;
import java.util.List;

/** Consolidates logic about opening bookmarks. */
public class BookmarkOpener {
    private final BookmarkModel mModel;
    private final Context mContext;
    private final ComponentName mComponentName;

    /**
     * @param model The bookmark model, used to query for bookmark urls and type.
     * @param context The android context, used to build the intent to open bookmarks.
     * @param componentName The name of the parent component, can be null on tablets.
     */
    public BookmarkOpener(
            BookmarkModel model, Context context, @Nullable ComponentName componentName) {
        mModel = model;
        mContext = context;
        mComponentName = componentName;
    }

    /**
     * Open the given id in the current tab.
     *
     * @param incognito Whether the bookmark should be opened in incognito mode.
     * @return Whether the bookmark id was successfully opened.
     */
    public boolean openBookmarkInCurrentTab(BookmarkId id, boolean incognito) {
        if (id == null) return false;
        BookmarkItem item = mModel.getBookmarkById(id);
        if (item == null) return false;
        maybeMarkReadingListItemAsRead(item);
        recordMetricsForOpenBookmarkInCurrentTab(item);

        Intent intent = createBasicOpenIntent(item, incognito);
        IntentHandler.startActivityForTrustedIntent(intent);
        return true;
    }

    /**
     * Open the given bookmarkIds in new tabs.
     *
     * @param bookmarkIds The bookmark ids to open.
     * @param incognito Whether the bookmarks should be opened in incognito mode.
     * @return Whether the bookmark ids were successfully opened.
     */
    public boolean openBookmarksInNewTabs(List<BookmarkId> bookmarkIds, boolean incognito) {
        if (bookmarkIds == null || bookmarkIds.size() == 0) return false;

        BookmarkItem firstItem = null;
        ArrayList<String> additionalUrls = new ArrayList<>();
        List<BookmarkItem> items = new ArrayList<>();
        for (BookmarkId id : bookmarkIds) {
            // Might need to check if this is a folder.
            if (id == null) continue;
            BookmarkItem item = mModel.getBookmarkById(id);
            if (item == null) continue;
            maybeMarkReadingListItemAsRead(item);

            if (firstItem == null) {
                firstItem = item;
            } else {
                additionalUrls.add(item.getUrl().getSpec());
            }

            // Collected for metrics to avoid additional JNI calls.
            items.add(item);
        }
        recordMetricsForOpenBookmarksInNewTabs(items);

        Intent intent = createBasicOpenIntent(firstItem, incognito);
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, incognito);
        intent.putExtra(IntentHandler.EXTRA_ADDITIONAL_URLS, additionalUrls);
        IntentHandler.startActivityForTrustedIntent(intent);
        return true;
    }

    private Intent createBasicOpenIntent(@NonNull BookmarkItem item, boolean incognito) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(item.getUrl().getSpec()));
        intent.putExtra(
                Browser.EXTRA_APPLICATION_ID, mContext.getApplicationContext().getPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PageTransition.AUTO_BOOKMARK);
        intent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_BOOKMARK_ID, item.getId().toString());
        intent.putExtra(IntentHandler.EXTRA_INCOGNITO_MODE, incognito);

        if (mComponentName != null) {
            ActivityUtils.setNonAliasedComponentForMainBrowsingActivity(intent, mComponentName);
        } else {
            // If the bookmark manager is shown in a tab on a phone (rather than in a separate
            // activity) the component name may be null. Send the intent through
            // ChromeLauncherActivity instead to avoid crashing. See crbug.com/615012.
            intent.setClass(mContext.getApplicationContext(), ChromeLauncherActivity.class);
        }

        // Reading list has special back button behavior which brings the reading list back up
        // as the first back action when viewing an item. This is driven by the FROM_READING_LIST
        // TabLaunchType.
        if (item.getId().getType() == BookmarkType.READING_LIST) {
            IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_READING_LIST);
            intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, incognito);
        }

        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }

    private void maybeMarkReadingListItemAsRead(@NonNull BookmarkItem item) {
        if (item.getId().getType() == BookmarkType.READING_LIST) {
            mModel.setReadStatusForReadingList(item.getId(), true);
        }
    }

    // Metrics

    private void recordMetricsForOpenBookmarkInCurrentTab(@NonNull BookmarkItem item) {
        RecordUserAction.record("MobileBookmarkManagerEntryOpened");
        recordTypeOpened(item, "Bookmarks.OpenBookmarkType");
        recordTimeSinceAdded(item, "Bookmarks.OpenBookmarkTimeInterval2.");
    }

    private void recordMetricsForOpenBookmarksInNewTabs(List<BookmarkItem> items) {
        RecordUserAction.record("MobileBookmarkManagerMultipleEntriesOpened");

        for (BookmarkItem item : items) {
            recordTypeOpened(item, "Bookmarks.MultipleOpened.OpenBookmarkType");
            recordTimeSinceAdded(item, "Bookmarks.MultipleOpened.OpenBookmarkTimeInterval2.");
        }
    }

    private String bookmarkTypeToHistogramSuffix(@BookmarkType int type) {
        switch (type) {
            case BookmarkType.NORMAL:
                return "Normal";
            case BookmarkType.PARTNER:
                return "Partner";
            case BookmarkType.READING_LIST:
                return "ReadingList";
        }
        assert false : "Unknown BookmarkType";
        return "";
    }

    private void recordTypeOpened(BookmarkItem item, String histogram) {
        RecordHistogram.recordEnumeratedHistogram(
                histogram, item.getId().getType(), BookmarkType.LAST + 1);
    }

    private void recordTimeSinceAdded(BookmarkItem item, String histogramPrefix) {
        RecordHistogram.recordCustomTimesHistogram(
                histogramPrefix + bookmarkTypeToHistogramSuffix(item.getId().getType()),
                System.currentTimeMillis() - item.getDateAdded(),
                1,
                DateUtils.DAY_IN_MILLIS * 30,
                50);
    }
}
