// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;

/**
 * A class holding static util functions for bookmark.
 */
public class BookmarkUtils {
    private static final String TAG = "BookmarkUtils";

    /**
     * If the tab has already been bookmarked, start {@link BookmarkEditActivity} for the
     * bookmark. If not, add the bookmark to bookmarkmodel, and show a snackbar notifying the user.
     *
     * Note: Takes ownership of bookmarkModel, and will call |destroy| on it when finished.
     *
     * @param existingBookmarkId The bookmark ID if the tab has already been bookmarked.
     * @param bookmarkModel The bookmark model.
     * @param tab The tab to add or edit a bookmark.
     * @param snackbarManager The SnackbarManager used to show the snackbar.
     * @param activity Current activity.
     * @param fromCustomTab boolean indicates whether it is called by Custom Tab.
     * @return Bookmark ID of the bookmark. Could be <code>null</code> if bookmark didn't exist
     *   and bookmark model failed to create it.
     */
    public static BookmarkId addOrEditBookmark(long existingBookmarkId, BookmarkModel bookmarkModel,
            Tab tab, SnackbarManager snackbarManager, Activity activity, boolean fromCustomTab) {
        if (existingBookmarkId != BookmarkId.INVALID_ID) {
            BookmarkId bookmarkId = new BookmarkId(existingBookmarkId, BookmarkType.NORMAL);
            startEditActivity(activity, bookmarkId);
            bookmarkModel.destroy();
            return bookmarkId;
        }

        BookmarkId bookmarkId =
                addBookmarkInternal(activity, bookmarkModel, tab.getTitle(), tab.getOriginalUrl());

        Snackbar snackbar = null;
        if (bookmarkId == null) {
            snackbar = Snackbar.make(activity.getString(R.string.bookmark_page_failed),
                    new SnackbarController() {
                        @Override
                        public void onDismissNoAction(Object actionData) { }

                        @Override
                        public void onAction(Object actionData) { }
                    }, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_BOOKMARK_ADDED)
                    .setSingleLine(false);
            RecordUserAction.record("EnhancedBookmarks.AddingFailed");
        } else {
            String folderName = bookmarkModel.getBookmarkTitle(
                    bookmarkModel.getBookmarkById(bookmarkId).getParentId());
            SnackbarController snackbarController =
                    createSnackbarControllerForEditButton(activity, bookmarkId);
            if (getLastUsedParent(activity) == null) {
                if (fromCustomTab) {
                    String packageLabel = BuildInfo.getInstance().hostPackageLabel;
                    snackbar = Snackbar.make(
                            activity.getString(R.string.bookmark_page_saved, packageLabel),
                            snackbarController, Snackbar.TYPE_ACTION, Snackbar.UMA_BOOKMARK_ADDED);
                } else {
                    snackbar = Snackbar.make(
                            activity.getString(R.string.bookmark_page_saved_default),
                            snackbarController, Snackbar.TYPE_ACTION, Snackbar.UMA_BOOKMARK_ADDED);
                }
            } else {
                snackbar = Snackbar.make(folderName, snackbarController, Snackbar.TYPE_ACTION,
                        Snackbar.UMA_BOOKMARK_ADDED)
                        .setTemplateText(activity.getString(R.string.bookmark_page_saved_folder));
            }
            snackbar.setSingleLine(false).setAction(activity.getString(R.string.bookmark_item_edit),
                    null);
        }
        snackbarManager.showSnackbar(snackbar);

        bookmarkModel.destroy();
        return bookmarkId;
    }

    /**
     * An internal version of {@link #addBookmarkSilently(Context, BookmarkModel, String, String)}.
     * Will reset last used parent if it fails to add a bookmark
     */
    private static BookmarkId addBookmarkInternal(
            Context context, BookmarkModel bookmarkModel, String title, String url) {
        BookmarkId parent = getLastUsedParent(context);
        BookmarkItem parentItem = null;
        if (parent != null) {
            parentItem = bookmarkModel.getBookmarkById(parent);
        }
        if (parent == null || parentItem == null || parentItem.isManaged()
                || !parentItem.isFolder()) {
            parent = bookmarkModel.getDefaultFolder();
        }
        BookmarkId bookmarkId =
                bookmarkModel.addBookmark(parent, bookmarkModel.getChildCount(parent), title, url);

        // TODO(lazzzis): remove log after bookmark sync is fixed, crbug.com/986978
        if (bookmarkId == null) {
            Log.e(TAG,
                    "Failed to add bookmarks: parentTypeAndId %s, defaultFolderTypeAndId %s, "
                            + "mobileFolderTypeAndId %s, parentEditable Managed isFolder %s,",
                    parent, bookmarkModel.getDefaultFolder(), bookmarkModel.getMobileFolderId(),
                    parentItem == null ? "null"
                                       : (parentItem.isEditable() + " " + parentItem.isManaged()
                                               + " " + parentItem.isFolder()));
            setLastUsedParent(context, bookmarkModel.getDefaultFolder());
        }
        return bookmarkId;
    }

    /**
     * Creates a snackbar controller for a case where "Edit" button is shown to edit the newly
     * created bookmark.
     */
    private static SnackbarController createSnackbarControllerForEditButton(
            final Activity activity, final BookmarkId bookmarkId) {
        return new SnackbarController() {
            @Override
            public void onDismissNoAction(Object actionData) {
                RecordUserAction.record("EnhancedBookmarks.EditAfterCreateButtonNotClicked");
            }

            @Override
            public void onAction(Object actionData) {
                RecordUserAction.record("EnhancedBookmarks.EditAfterCreateButtonClicked");
                startEditActivity(activity, bookmarkId);
            }
        };
    }

    /**
     * Shows bookmark main UI.
     * @param activity An activity to start the manager with.
     */
    public static void showBookmarkManager(ChromeActivity activity) {
        ThreadUtils.assertOnUiThread();
        String url = getFirstUrlToLoad(activity);

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)) {
            openUrl(activity, url, activity.getComponentName());
        } else {
            Intent intent = new Intent(activity, BookmarkActivity.class);
            intent.setData(Uri.parse(url));
            intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
            activity.startActivity(intent);
        }
    }

    /**
     * The initial url the bookmark manager shows depends some experiments we run.
     */
    private static String getFirstUrlToLoad(Activity activity) {
        String lastUsedUrl = getLastUsedUrl(activity);
        return TextUtils.isEmpty(lastUsedUrl) ? UrlConstants.BOOKMARKS_URL : lastUsedUrl;
    }

    /**
     * Saves the last used url to preference. The saved url will be later queried by
     * {@link #getLastUsedUrl(Context)}
     */
    static void setLastUsedUrl(Context context, String url) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL, url);
    }

    /**
     * Fetches url representing the user's state last time they close the bookmark manager.
     */
    @VisibleForTesting
    static String getLastUsedUrl(Context context) {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL, UrlConstants.BOOKMARKS_URL);
    }

    /**
     * Save the last used {@link BookmarkId} as a folder to put new bookmarks to.
     */
    static void setLastUsedParent(Context context, BookmarkId bookmarkId) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT, bookmarkId.toString());
    }

    /**
     * @return The parent {@link BookmarkId} that the user used the last time or null if the user
     *         has never selected a parent folder to use.
     */
    static BookmarkId getLastUsedParent(Context context) {
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        if (!preferences.contains(ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT)) return null;

        return BookmarkId.getBookmarkIdFromString(
                preferences.readString(ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT, null));
    }

    /** Starts an {@link BookmarkEditActivity} for the given {@link BookmarkId}. */
    public static void startEditActivity(Context context, BookmarkId bookmarkId) {
        RecordUserAction.record("MobileBookmarkManagerEditBookmark");
        Intent intent = new Intent(context, BookmarkEditActivity.class);
        intent.putExtra(BookmarkEditActivity.INTENT_BOOKMARK_ID, bookmarkId.toString());
        if (context instanceof BookmarkActivity) {
            ((BookmarkActivity) context).startActivityForResult(
                    intent, BookmarkActivity.EDIT_BOOKMARK_REQUEST_CODE);
        } else {
            context.startActivity(intent);
        }
    }

    /**
     * Opens a bookmark and reports UMA.
     * @param context The current context used to launch the intent.
     * @param openBookmarkComponentName The component to use when opening a bookmark.
     * @param model Bookmarks model to manage the bookmark.
     * @param bookmarkId ID of the bookmark to be opened.
     * @return Whether the bookmark was successfully opened.
     */
    public static boolean openBookmark(Context context, ComponentName openBookmarkComponentName,
            BookmarkModel model, BookmarkId bookmarkId) {
        if (model.getBookmarkById(bookmarkId) == null) return false;

        RecordUserAction.record("MobileBookmarkManagerEntryOpened");
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.OpenBookmarkType", bookmarkId.getType(), BookmarkType.LAST + 1);

        String url = model.getBookmarkById(bookmarkId).getUrl();
        openUrl(context, url, openBookmarkComponentName);

        return true;
    }

    /**
     * @param context {@link Context} used to retrieve the drawable.
     * @return A {@link TintedDrawable} to use for displaying bookmark folders.
     */
    public static TintedDrawable getFolderIcon(Context context) {
        return TintedDrawable.constructTintedDrawable(
                context, R.drawable.ic_folder_blue_24dp, getFolderIconTint());
    }

    /**
     * @return The tint used on the bookmark folder icon.
     */
    public static int getFolderIconTint() {
        return R.color.default_icon_color_tint_list;
    }

    private static void openUrl(Context context, String url, ComponentName componentName) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.putExtra(
                Browser.EXTRA_APPLICATION_ID, context.getApplicationContext().getPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PageTransition.AUTO_BOOKMARK);

        if (componentName != null) {
            ChromeTabbedActivity.setNonAliasedComponent(intent, componentName);
        } else {
            // If the bookmark manager is shown in a tab on a phone (rather than in a separate
            // activity) the component name may be null. Send the intent through
            // ChromeLauncherActivity instead to avoid crashing. See crbug.com/615012.
            intent.setClass(context.getApplicationContext(), ChromeLauncherActivity.class);
        }

        IntentHandler.startActivityForTrustedIntent(intent);
    }

    /**
     * Closes the {@link BookmarkActivity} on Phone. Does nothing on tablet.
     */
    public static void finishActivityOnPhone(Context context) {
        if (context instanceof BookmarkActivity) {
            ((Activity) context).finish();
        }
    }
}
