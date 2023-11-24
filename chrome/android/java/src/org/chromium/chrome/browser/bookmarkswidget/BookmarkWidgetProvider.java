// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkswidget;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.widget.RemoteViews;

import com.google.android.apps.chrome.appwidget.bookmarks.BookmarkThumbnailWidgetProvider;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;

/** Widget that shows a preview of the user's bookmarks. */
public class BookmarkWidgetProvider extends AppWidgetProvider {
    private static final String ACTION_BOOKMARK_APPWIDGET_UPDATE_SUFFIX =
            ".BOOKMARK_APPWIDGET_UPDATE";
    private static final int ICONS_ONLY_THRESHOLD_WIDTH_DP = 110;

    @Override
    public void onReceive(Context context, Intent intent) {
        // Handle bookmark-specific updates ourselves because they might be
        // coming in without extras, which AppWidgetProvider then blocks.
        final String action = intent.getAction();
        if (getBookmarkAppWidgetUpdateAction(context).equals(action)) {
            AppWidgetManager appWidgetManager = AppWidgetManager.getInstance(context);
            if (intent.hasExtra(AppWidgetManager.EXTRA_APPWIDGET_ID)) {
                performUpdate(
                        context,
                        appWidgetManager,
                        new int[] {
                            IntentUtils.safeGetIntExtra(
                                    intent, AppWidgetManager.EXTRA_APPWIDGET_ID, -1)
                        });
            } else {
                performUpdate(
                        context,
                        appWidgetManager,
                        appWidgetManager.getAppWidgetIds(getComponentName(context)));
            }
        } else {
            super.onReceive(context, intent);
        }
    }

    @Override
    public void onUpdate(Context context, AppWidgetManager manager, int[] ids) {
        super.onUpdate(context, manager, ids);
        performUpdate(context, manager, ids);
    }

    @Override
    public void onAppWidgetOptionsChanged(
            Context context,
            AppWidgetManager appWidgetManager,
            int appWidgetId,
            Bundle newOptions) {
        // Update the widget after it's resized in case it's crossed the threshold between icon-
        // only mode and regular mode.
        performUpdate(context, appWidgetManager, new int[] {appWidgetId});
    }

    @Override
    public void onDeleted(Context context, int[] appWidgetIds) {
        super.onDeleted(context, appWidgetIds);
        for (int widgetId : appWidgetIds) {
            BookmarkWidgetServiceImpl.deleteWidgetState(widgetId);
        }
        removeOrphanedStates(context);
    }

    @Override
    public void onDisabled(Context context) {
        super.onDisabled(context);
        removeOrphanedStates(context);
    }

    /** Refreshes all Chrome Bookmark widgets. */
    public static void refreshAllWidgets() {
        Context context = ContextUtils.getApplicationContext();
        if (AppWidgetManager.getInstance(context) == null) return;

        context.sendBroadcast(
                new Intent(
                        getBookmarkAppWidgetUpdateAction(context),
                        null,
                        context,
                        BookmarkThumbnailWidgetProvider.class));
    }

    static String getBookmarkAppWidgetUpdateAction(Context context) {
        return context.getPackageName() + ACTION_BOOKMARK_APPWIDGET_UPDATE_SUFFIX;
    }

    /** Checks for any states that may have not received onDeleted. */
    private void removeOrphanedStates(Context context) {
        AppWidgetManager wm = AppWidgetManager.getInstance(context);
        int[] ids = wm.getAppWidgetIds(getComponentName(context));
        for (int id : ids) {
            BookmarkWidgetServiceImpl.deleteWidgetState(id);
        }
    }

    private void performUpdate(
            Context context, AppWidgetManager appWidgetManager, int[] appWidgetIds) {
        for (int appWidgetId : appWidgetIds) {
            Intent updateIntent = new Intent(context, BookmarkWidgetService.class);
            updateIntent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, appWidgetId);
            updateIntent.setData(Uri.parse(updateIntent.toUri(Intent.URI_INTENT_SCHEME)));

            int layoutId =
                    shouldShowIconsOnly(appWidgetManager, appWidgetId)
                            ? R.layout.bookmark_widget_icons_only
                            : R.layout.bookmark_widget;
            RemoteViews views = new RemoteViews(context.getPackageName(), layoutId);
            views.setRemoteAdapter(R.id.bookmarks_list, updateIntent);

            appWidgetManager.notifyAppWidgetViewDataChanged(appWidgetId, R.id.bookmarks_list);
            Intent ic = new Intent(context, BookmarkWidgetProxy.class);
            IntentUtils.addTrustedIntentExtras(ic);
            ic.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            views.setPendingIntentTemplate(
                    R.id.bookmarks_list,
                    PendingIntent.getActivity(
                            context,
                            0,
                            ic,
                            PendingIntent.FLAG_UPDATE_CURRENT
                                    | IntentUtils.getPendingIntentMutabilityFlag(true)));
            appWidgetManager.updateAppWidget(appWidgetId, views);
        }
    }

    private boolean shouldShowIconsOnly(AppWidgetManager appWidgetManager, int appWidgetId) {
        int widthDp =
                appWidgetManager
                        .getAppWidgetOptions(appWidgetId)
                        .getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH);
        return widthDp < ICONS_ONLY_THRESHOLD_WIDTH_DP;
    }

    /**
     * Build {@link ComponentName} describing this specific
     * {@link AppWidgetProvider}
     */
    private static ComponentName getComponentName(Context context) {
        return new ComponentName(context, BookmarkThumbnailWidgetProvider.class);
    }
}
