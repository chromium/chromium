// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkswidget;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.RemoteViews;
import android.widget.RemoteViewsService.RemoteViewsFactory;

import androidx.annotation.BinderThread;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;

import com.google.android.apps.chrome.appwidget.bookmarks.BookmarkThumbnailWidgetProvider;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.base.SplitCompatRemoteViewsService;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkNodeMaskBit;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkQueryHandler;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.CountDownLatch;

/**
 * Service to support the bookmarks widget.
 *
 * <p>This provides the list of bookmarks to show in the widget via a RemoteViewsFactory (the
 * RemoteViews equivalent of an Adapter), and updates the widget when the bookmark model changes.
 *
 * <p>Threading note: Be careful! Android calls some methods in this class on the UI thread and
 * others on (multiple) binder threads. Additionally, all interaction with the BookmarkModel must
 * happen on the UI thread. To keep the situation clear, every non-static method is annotated with
 * either {@link UiThread} or {@link BinderThread}.
 */
@NullMarked
public class BookmarkWidgetServiceImpl extends SplitCompatRemoteViewsService.Impl {
    private static final String TAG = "BookmarkWidget";
    private static final String ACTION_CHANGE_FOLDER_SUFFIX = ".CHANGE_FOLDER";
    private static final String PREF_CURRENT_FOLDER = "bookmarkswidget.current_folder";
    private static final String EXTRA_FOLDER_ID = "folderId";
    private static final String EXTRA_FOLDER_TITLE = "folderTitle";
    private static final String SET_COLOR_FILTER = "setColorFilter";

    // Store map of WidgetId -> Map<BookmarkId, BookmarkItem> for multiple instances of widget.
    private static final Map<Integer, Map<String, BookmarkItem>> sBookmarkParentMaps =
            new HashMap<>();

    @UiThread
    @Override
    public @Nullable RemoteViewsFactory onGetViewFactory(Intent intent) {
        int widgetId = IntentUtils.safeGetIntExtra(intent, AppWidgetManager.EXTRA_APPWIDGET_ID, -1);
        if (widgetId < 0) {
            Log.w(TAG, "Missing EXTRA_APPWIDGET_ID!");
            return null;
        }
        return new BookmarkAdapter(assumeNonNull(getService()), widgetId);
    }

    static String getChangeFolderAction() {
        return ContextUtils.getApplicationContext().getPackageName() + ACTION_CHANGE_FOLDER_SUFFIX;
    }

    static SharedPreferences getWidgetState(int widgetId) {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(
                        String.format(Locale.US, "widgetState-%d", widgetId), Context.MODE_PRIVATE);
    }

    static void deleteWidgetState(int widgetId) {
        SharedPreferences preferences = getWidgetState(widgetId);
        if (preferences != null) preferences.edit().clear().apply();
    }

    static int getIconColor(Context context) {
        ContextThemeWrapper wrapper =
                new ContextThemeWrapper(context, R.style.Theme_Chromium_Widget);

        return SemanticColorUtils.getDefaultIconColorSecondary(wrapper);
    }

    static void changeFolder(Intent intent) {
        int widgetId = IntentUtils.safeGetIntExtra(intent, AppWidgetManager.EXTRA_APPWIDGET_ID, -1);
        String serializedFolder = IntentUtils.safeGetStringExtra(intent, EXTRA_FOLDER_ID);
        if (widgetId < 0 || serializedFolder == null) return;

        SharedPreferences prefs = getWidgetState(widgetId);
        prefs.edit().putString(PREF_CURRENT_FOLDER, serializedFolder).apply();

        // Provide an instantaneous partial update to the up_navigation (folder title and back
        // arrow) to avoid UI lag while the full bookmark list loads asynchronously. We cannot
        // access the BookmarkModel on this thread, so we rely on navigation data pre-packaged
        // through the parent bookmark folder map.
        String folderTitle = IntentUtils.safeGetStringExtra(intent, EXTRA_FOLDER_TITLE);
        BookmarkItem parentFolder;
        synchronized (sBookmarkParentMaps) {
            Map<String, BookmarkItem> parentMap = sBookmarkParentMaps.get(widgetId);
            parentFolder = (parentMap == null) ? null : parentMap.get(serializedFolder);
        }
        String parentFolderId = null;
        String parentFolderTitle = null;
        if (parentFolder != null) {
            parentFolderId = parentFolder.getId().toString();
            parentFolderTitle = parentFolder.getTitle();
        }

        Context context = ContextUtils.getApplicationContext();
        int iconColor = getIconColor(context);

        RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.bookmark_widget);
        views.setTextViewText(R.id.folder_title, folderTitle);

        // Add parent folder id and title to the up_navigation intent.
        if (parentFolderId != null && parentFolderTitle != null) {
            Intent upIntent =
                    new Intent(context, BookmarkWidgetProxy.class)
                            .setAction(getChangeFolderAction())
                            .putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, widgetId)
                            .putExtra(EXTRA_FOLDER_ID, parentFolderId)
                            .putExtra(EXTRA_FOLDER_TITLE, parentFolderTitle);
            IntentUtils.addTrustedIntentExtras(upIntent);

            PendingIntent pendingIntent =
                    PendingIntent.getActivity(
                            context,
                            widgetId,
                            upIntent,
                            PendingIntent.FLAG_UPDATE_CURRENT
                                    | IntentUtils.getPendingIntentMutabilityFlag(true));
            views.setOnClickPendingIntent(R.id.up_navigation, pendingIntent);
            views.setInt(R.id.back_button, SET_COLOR_FILTER, iconColor);
            views.setViewVisibility(R.id.up_navigation, View.VISIBLE);
        } else {
            views.setViewVisibility(R.id.up_navigation, View.GONE);
        }
        views.setViewVisibility(R.id.bookmarks_list, View.GONE);
        AppWidgetManager.getInstance(context).partiallyUpdateAppWidget(widgetId, views);
        redrawWidget(widgetId);
    }

    @VisibleForTesting
    static RemoteViews createBookmarkWidgetRemoteView(
            Context context,
            int widgetId,
            BookmarkItem folder,
            @Nullable BookmarkItem parent,
            int iconColor) {

        RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.bookmark_widget);
        views.setTextViewText(R.id.folder_title, folder.getTitle());

        if (parent != null) {
            Intent upIntent =
                    new Intent(context, BookmarkWidgetProxy.class)
                            .setAction(getChangeFolderAction())
                            .putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, widgetId)
                            .putExtra(EXTRA_FOLDER_ID, parent.getId().toString())
                            .putExtra(EXTRA_FOLDER_TITLE, parent.getTitle());

            IntentUtils.addTrustedIntentExtras(upIntent);

            PendingIntent pendingIntent =
                    PendingIntent.getActivity(
                            context,
                            widgetId,
                            upIntent,
                            PendingIntent.FLAG_UPDATE_CURRENT
                                    | IntentUtils.getPendingIntentMutabilityFlag(true));
            views.setOnClickPendingIntent(R.id.up_navigation, pendingIntent);
            views.setInt(R.id.back_button, SET_COLOR_FILTER, iconColor);
            views.setViewVisibility(R.id.up_navigation, View.VISIBLE);
        } else {
            views.setViewVisibility(R.id.up_navigation, View.GONE);
        }
        return views;
    }

    /**
     * Redraws / refreshes a bookmark widget.
     *
     * @param widgetId The ID of the widget to redraw.
     */
    static void redrawWidget(int widgetId) {
        AppWidgetManager.getInstance(ContextUtils.getApplicationContext())
                .notifyAppWidgetViewDataChanged(widgetId, R.id.bookmarks_list);
    }

    /** Called when the BookmarkLoader has finished loading the bookmark folder. */
    private interface BookmarkLoaderCallback {
        @UiThread
        void onBookmarksLoaded(
                @Nullable BookmarkItem folder,
                @Nullable BookmarkItem parent,
                @Nullable List<BookmarkListEntry> entries,
                Map<BookmarkId, Bitmap> favicons);
    }

    /**
     * Loads bookmark data asynchronously, and returns the result via BookmarkLoaderCallback.
     *
     * <p>This class must be used only on the UI thread.
     */
    private static class BookmarkLoader {
        private BookmarkLoaderCallback mCallback;
        private @Nullable BookmarkItem mFolder;
        private @Nullable BookmarkItem mParent;
        private @Nullable List<BookmarkListEntry> mEntries;
        private final Map<BookmarkId, Bitmap> mFavicons = new HashMap<>();
        private BookmarkModel mBookmarkModel;
        private LargeIconBridge mLargeIconBridge;
        private ImprovedBookmarkQueryHandler mQueryHandler;
        private RoundedIconGenerator mIconGenerator;
        private int mMinIconSizeDp;
        private int mDisplayedIconSize;
        private int mRemainingTaskCount;

        @UiThread
        @Initializer
        public void initialize(
                Context context, final BookmarkId folderId, BookmarkLoaderCallback callback) {
            mCallback = callback;

            Resources res = context.getResources();
            mLargeIconBridge = new LargeIconBridge(ProfileManager.getLastUsedRegularProfile());
            mMinIconSizeDp = (int) res.getDimension(R.dimen.default_favicon_min_size);
            mDisplayedIconSize = res.getDimensionPixelSize(R.dimen.default_favicon_size);
            mIconGenerator = FaviconUtils.createRoundedRectangleIconGenerator(context);

            mRemainingTaskCount = 1;
            mBookmarkModel =
                    BookmarkModel.getForProfile(ProfileManager.getLastUsedRegularProfile());
            mQueryHandler =
                    new ImprovedBookmarkQueryHandler(
                            mBookmarkModel,
                            new BookmarkUiPrefs(ChromeSharedPreferences.getInstance()),
                            /* shoppingService */ null,
                            BookmarkNodeMaskBit.NONE);
            mBookmarkModel.finishLoadingBookmarkModel(
                    new Runnable() {
                        @Override
                        public void run() {
                            loadBookmarks(folderId);
                        }
                    });
        }

        @UiThread
        private void loadBookmarks(BookmarkId folderId) {
            // Load the requested folder if it exists. Otherwise, fall back to the default folder.
            if (folderId != null) {
                mFolder = mBookmarkModel.getBookmarkById(folderId);
            }
            if (mFolder == null) {
                folderId = mBookmarkModel.getDefaultBookmarkFolder();
                mFolder = mBookmarkModel.getBookmarkById(folderId);
            }
            assertNonNull(folderId);
            assertNonNull(mFolder);

            mParent = mBookmarkModel.getBookmarkById(mFolder.getParentId());

            mEntries = mQueryHandler.buildBookmarkListForParent(folderId, null);

            for (BookmarkListEntry entry : mEntries) {
                BookmarkItem item = entry.getBookmarkItem();
                if (item != null && !item.isFolder()) {
                    loadFavicon(item);
                }
            }

            taskFinished();
        }

        @UiThread
        private void loadFavicon(BookmarkItem bookmarkItem) {
            if (bookmarkItem == null || bookmarkItem.isFolder()) return;

            mRemainingTaskCount++;
            LargeIconCallback callback =
                    new LargeIconCallback() {
                        @Override
                        public void onLargeIconAvailable(
                                @Nullable Bitmap icon,
                                int fallbackColor,
                                boolean isFallbackColorDefault,
                                @IconType int iconType) {
                            if (icon == null) {
                                mIconGenerator.setBackgroundColor(fallbackColor);
                                icon = mIconGenerator.generateIconForUrl(bookmarkItem.getUrl());
                            } else {
                                icon =
                                        Bitmap.createScaledBitmap(
                                                icon, mDisplayedIconSize, mDisplayedIconSize, true);
                            }
                            mFavicons.put(bookmarkItem.getId(), icon);
                            taskFinished();
                        }
                    };
            mLargeIconBridge.getLargeIconForUrl(bookmarkItem.getUrl(), mMinIconSizeDp, callback);
        }

        @UiThread
        private void taskFinished() {
            mRemainingTaskCount--;
            if (mRemainingTaskCount == 0) {
                mCallback.onBookmarksLoaded(mFolder, mParent, mEntries, mFavicons);
                destroy();
            }
        }

        @UiThread
        private void destroy() {
            mLargeIconBridge.destroy();
        }
    }

    /** Provides the RemoteViews, one per bookmark, to be shown in the widget. */
    @VisibleForTesting
    static class BookmarkAdapter implements RemoteViewsFactory, SystemNightModeMonitor.Observer {
        // Can be accessed on any thread
        private final Context mContext;
        private final int mWidgetId;
        private final SharedPreferences mPreferences;
        private int mIconColor;

        // Accessed only on the UI thread
        private BookmarkModel mBookmarkModel;

        // Accessed only on binder threads.
        private @Nullable BookmarkItem mCurrentFolder;
        private @Nullable BookmarkItem mParentFolder;
        private final List<BookmarkListEntry> mEntries;
        private final Map<BookmarkId, Bitmap> mFavicons;

        @UiThread
        public BookmarkAdapter(Context context, int widgetId) {
            mContext = context;
            mWidgetId = widgetId;
            mPreferences = getWidgetState(mWidgetId);
            mIconColor = getIconColor(mContext);
            SystemNightModeMonitor.getInstance().addObserver(this);
            mEntries = new ArrayList<>();
            mFavicons = new HashMap<>();
        }

        @UiThread
        @Override
        @Initializer
        public void onCreate() {
            // Required to be applied here redundantly to prevent crashes in the cases where the
            // package data is deleted or the Chrome application forced to stop.
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
            if (isWidgetNewlyCreated()) {
                RecordUserAction.record("BookmarkNavigatorWidgetAdded");
            }

            mBookmarkModel =
                    BookmarkModel.getForProfile(ProfileManager.getLastUsedRegularProfile());
            mBookmarkModel.addObserver(
                    new BookmarkModelObserver() {
                        @Override
                        public void bookmarkModelLoaded() {
                            // Do nothing. No need to refresh.
                        }

                        @Override
                        public void bookmarkModelChanged() {
                            redrawWidget(mWidgetId);
                        }
                    });
        }

        @UiThread
        private boolean isWidgetNewlyCreated() {
            // This method relies on the fact that PREF_CURRENT_FOLDER is not yet
            // set when onCreate is called for a newly created widget.
            String serializedFolder = mPreferences.getString(PREF_CURRENT_FOLDER, null);
            return serializedFolder == null;
        }

        @UiThread
        private void refreshWidget() {
            mContext.sendBroadcast(
                    new Intent(
                                    BookmarkWidgetProvider.getBookmarkAppWidgetUpdateAction(
                                            mContext),
                                    null,
                                    mContext,
                                    BookmarkThumbnailWidgetProvider.class)
                            .putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, mWidgetId));
        }

        // ---------------------------------------------------------------- //
        // Methods below this line are called on binder threads.            //
        // ---------------------------------------------------------------- //
        // Different methods may be called on *different* binder threads,   //
        // but the system ensures that the effects of each method call will //
        // be visible before the next method is called. Thus, additional    //
        // synchronization is not needed when accessing mCurrentFolder.     //
        // ---------------------------------------------------------------- //

        @BinderThread
        @Override
        public void onDestroy() {
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        SystemNightModeMonitor.getInstance().removeObserver(this);
                    });
            synchronized (sBookmarkParentMaps) {
                sBookmarkParentMaps.remove(mWidgetId);
            }
            deleteWidgetState(mWidgetId);
        }

        @BinderThread
        @Override
        public void onDataSetChanged() {
            updateBookmarkList();
        }

        @BinderThread
        private void updateBookmarkList() {
            BookmarkId folderId =
                    BookmarkId.getBookmarkIdFromString(
                            mPreferences.getString(PREF_CURRENT_FOLDER, null));

            // Blocks until bookmarks are loaded from the UI thread.
            loadBookmarks(folderId);
            updateBookmarkWidgetView();

            if (mCurrentFolder != null) {
                mPreferences
                        .edit()
                        .putString(PREF_CURRENT_FOLDER, mCurrentFolder.getId().toString())
                        .apply();
            }
        }

        @BinderThread
        private void updateBookmarkWidgetView() {
            assertNonNull(mCurrentFolder);
            RemoteViews views =
                    createBookmarkWidgetRemoteView(
                            mContext, mWidgetId, mCurrentFolder, mParentFolder, mIconColor);
            views.setViewVisibility(R.id.bookmarks_list, View.VISIBLE);
            AppWidgetManager.getInstance(mContext).partiallyUpdateAppWidget(mWidgetId, views);
        }

        @BinderThread
        private void loadBookmarks(final BookmarkId folderId) {
            final CountDownLatch latch = new CountDownLatch(1);
            // A reference of BookmarkLoader is needed in binder thread to
            // prevent it from being garbage collected.
            final BookmarkLoader bookmarkLoader = new BookmarkLoader();
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        bookmarkLoader.initialize(
                                mContext,
                                folderId,
                                new BookmarkLoaderCallback() {
                                    @Override
                                    public void onBookmarksLoaded(
                                            @Nullable BookmarkItem folder,
                                            @Nullable BookmarkItem parent,
                                            @Nullable List<BookmarkListEntry> entries,
                                            Map<BookmarkId, Bitmap> favicons) {
                                        mCurrentFolder = folder;
                                        mParentFolder = parent;
                                        mEntries.clear();
                                        if (entries != null) {
                                            mEntries.addAll(entries);
                                        }
                                        mFavicons.clear();
                                        mFavicons.putAll(favicons);

                                        synchronized (sBookmarkParentMaps) {
                                            Map<String, BookmarkItem> parentMap =
                                                    sBookmarkParentMaps.get(mWidgetId);
                                            if (parentMap == null) {
                                                parentMap = new HashMap<>();
                                                sBookmarkParentMaps.put(mWidgetId, parentMap);
                                            }

                                            if (mCurrentFolder != null) {
                                                parentMap.put(
                                                        mCurrentFolder.getId().toString(),
                                                        mParentFolder);
                                            }
                                            if (entries != null && mCurrentFolder != null) {
                                                for (BookmarkListEntry entry : entries) {
                                                    BookmarkItem item = entry.getBookmarkItem();
                                                    if (item != null && item.isFolder()) {
                                                        parentMap.put(
                                                                item.getId().toString(),
                                                                mCurrentFolder);
                                                    }
                                                }
                                            }
                                        }
                                        latch.countDown();
                                    }
                                });
                    });
            try {
                latch.await();
            } catch (InterruptedException e) {
                Log.e(TAG, "Unable to load bookmarks", e);
            }
        }

        private @Nullable BookmarkListEntry getBookmarkAtPosition(int position) {
            if (mEntries == null || position < 0 || position >= mEntries.size()) {
                return null;
            }
            return mEntries.get(position);
        }

        @BinderThread
        @Override
        public int getViewTypeCount() {
            return 2;
        }

        @BinderThread
        @Override
        public boolean hasStableIds() {
            return false;
        }

        @BinderThread
        @Override
        public int getCount() {
            // On some Sony devices, getCount() could be called before onDatasetChanged()
            // returns. If it happens, refresh widget until the bookmarks are all loaded.
            if (mCurrentFolder == null
                    || !mPreferences
                            .getString(PREF_CURRENT_FOLDER, "")
                            .equals(mCurrentFolder.getId().toString())) {
                PostTask.runOrPostTask(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            refreshWidget();
                        });
            }
            if (mCurrentFolder == null) {
                return 0;
            }
            return mEntries.size();
        }

        @BinderThread
        @Override
        public long getItemId(int position) {
            BookmarkListEntry entry = getBookmarkAtPosition(position);
            if (entry == null) return BookmarkId.INVALID_FOLDER_ID;
            if (entry.getBookmarkItem() != null) {
                return entry.getBookmarkItem().getId().getId();
            }

            // Bookmark widget section headers don't have stable IDs.
            return position;
        }

        @BinderThread
        @Override
        public RemoteViews getLoadingView() {
            return new RemoteViews(mContext.getPackageName(), R.layout.bookmark_widget_item);
        }

        @BinderThread
        @Override
        public @Nullable RemoteViews getViewAt(int position) {
            BookmarkListEntry entry = getBookmarkAtPosition(position);
            if (entry == null) return getLoadingView();

            if (entry.getViewType() == BookmarkListEntry.ViewType.SECTION_HEADER) {
                return createSectionHeaderView(entry, position);
            }

            return createBookmarkView(entry);
        }

        @Override
        public void onSystemNightModeChanged() {
            mIconColor = getIconColor(mContext);
            redrawWidget(mWidgetId);
        }

        private RemoteViews createSectionHeaderView(BookmarkListEntry entry, int position) {
            RemoteViews headerViews =
                    new RemoteViews(
                            mContext.getPackageName(), R.layout.bookmark_widget_section_header);
            headerViews.setTextViewText(R.id.title, entry.getTitle(mContext.getResources()));
            // Headers are not clickable.
            Intent emptyIntent = new Intent();
            PendingIntent pendingIntent =
                    PendingIntent.getBroadcast(
                            mContext, position, emptyIntent, PendingIntent.FLAG_IMMUTABLE);
            headerViews.setOnClickPendingIntent(R.id.title, pendingIntent);
            return headerViews;
        }

        private RemoteViews createBookmarkView(BookmarkListEntry entry) {
            BookmarkItem item = entry.getBookmarkItem();
            assertNonNull(item);

            // All other view types are rendered as bookmark widget item.
            RemoteViews views =
                    new RemoteViews(mContext.getPackageName(), R.layout.bookmark_widget_item);

            // Set the title of the bookmark. Use the url as a backup.
            String title = entry.getTitle(mContext.getResources());
            views.setTextViewText(
                    R.id.title, TextUtils.isEmpty(title) ? item.getUrl().getSpec() : title);

            Intent fillIn;

            if (item.isFolder()) {
                views.setInt(R.id.favicon, SET_COLOR_FILTER, mIconColor);
                views.setImageViewResource(R.id.favicon, R.drawable.ic_folder_blue_24dp);

                fillIn =
                        new Intent(getChangeFolderAction())
                                .putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, mWidgetId)
                                .putExtra(EXTRA_FOLDER_ID, item.getId().toString())
                                .putExtra(EXTRA_FOLDER_TITLE, item.getTitle());
            } else {
                // Clear any color filter so that it doesn't cover the favicon bitmap.
                views.setInt(R.id.favicon, SET_COLOR_FILTER, 0);
                views.setImageViewBitmap(R.id.favicon, mFavicons.get(item.getId()));

                fillIn = new Intent(Intent.ACTION_VIEW);
                fillIn.putExtra(
                        IntentHandler.EXTRA_PAGE_TRANSITION_BOOKMARK_ID, item.getId().toString());
                if (!item.getUrl().isEmpty()) {
                    fillIn.addCategory(Intent.CATEGORY_BROWSABLE)
                            .setData(Uri.parse(item.getUrl().getSpec()));
                } else {
                    fillIn.addCategory(Intent.CATEGORY_LAUNCHER);
                }
            }
            views.setViewVisibility(R.id.favicon, View.VISIBLE);
            views.setOnClickFillInIntent(R.id.list_item, fillIn);
            return views;
        }
    }
}
