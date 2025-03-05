// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.LocaleList;
import android.os.Looper;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.UiUtils;
import org.chromium.url.GURL;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.concurrent.TimeUnit;

/** A class holding static util functions for bookmark. */
// TODO(crbug.com/400793886): Audit arg ordering for functions.
public class BookmarkUtils {
    private static final String TAG = "BookmarkUtils";
    private static final int READING_LIST_SESSION_LENGTH_MS = (int) TimeUnit.HOURS.toMillis(1);

    private static Boolean sReadingListSupportedForTesting;

    /**
     * If the tab has already been bookmarked, start {@link BookmarkEditActivity} for the normal
     * bookmark or show the reading list page for reading list bookmark. If not, add the bookmark to
     * {@link BookmarkModel}, and show a snackbar notifying the user.
     *
     * @param existingBookmarkItem The {@link BookmarkItem} if the tab has already been bookmarked.
     * @param bookmarkModel The bookmark model.
     * @param tab The tab to add or edit a bookmark.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     * @param activity Current activity.
     * @param bookmarkType Type of the added bookmark.
     * @param callback Invoked with the resulting bookmark ID, which could be null if unsuccessful.
     * @param fromExplicitTrackUi Whether the bookmark was added directly from a tracking ui (e.g.
     *     the shopping "track price" button).
     */
    public static void addOrEditBookmark(
            @Nullable BookmarkItem existingBookmarkItem,
            BookmarkModel bookmarkModel,
            Tab tab,
            BottomSheetController bottomSheetController,
            Activity activity,
            @BookmarkType int bookmarkType,
            Callback<BookmarkId> callback,
            boolean fromExplicitTrackUi,
            BookmarkManagerOpener bookmarkManagerOpener) {
        assert bookmarkModel.isBookmarkModelLoaded();
        if (existingBookmarkItem != null) {
            bookmarkManagerOpener.startEditActivity(
                    activity, tab.getProfile(), existingBookmarkItem.getId());
            callback.onResult(existingBookmarkItem.getId());
            return;
        }

        BookmarkId parent = null;
        if (fromExplicitTrackUi) {
            // If account bookmarks are enabled and active, they take precedence, otherwise fall
            // back to the local-or-syncable mobile folder, e.g. for users that have
            // sync-the-feature enabled.
            parent =
                    bookmarkModel.areAccountBookmarkFoldersActive()
                            ? bookmarkModel.getAccountMobileFolderId()
                            : bookmarkModel.getMobileFolderId();
        }

        BookmarkId newBookmarkId =
                addBookmarkInternal(
                        activity,
                        tab.getProfile(),
                        bookmarkModel,
                        tab.getTitle(),
                        tab.getOriginalUrl(),
                        parent,
                        bookmarkType);
        showSaveFlow(
                activity,
                bottomSheetController,
                tab.getProfile(),
                newBookmarkId,
                fromExplicitTrackUi,
                /* wasBookmarkMoved= */ false,
                /* isNewBookmark= */ true,
                bookmarkManagerOpener);
        callback.onResult(newBookmarkId);
    }

    /**
     * Shows the bookmark save flow with the given {@link BookmarkId}.
     *
     * @param activity The current Activity.
     * @param bottomSheetController The BottomSheetController, used to show the save flow.
     * @param profile The profile currently used.
     * @param bookmarkId The BookmarkId to show the save flow for. Can be null in some cases (e.g. a
     *     bookmark fails to be added) and in this case, the function writes to logcat and exits
     *     without any action.
     * @param fromExplicitTrackUi Whether the bookmark was added from the explicit UI (e.g. the
     *     price-track menu item).
     * @param wasBookmarkMoved Whether the save flow is shown as a result of a moved bookmark.
     * @param isNewBookmark Whether the bookmark is newly created.
     */
    static void showSaveFlow(
            @NonNull Activity activity,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull Profile profile,
            @Nullable BookmarkId bookmarkId,
            boolean fromExplicitTrackUi,
            boolean wasBookmarkMoved,
            boolean isNewBookmark,
            BookmarkManagerOpener bookmarkManagerOpener) {
        if (bookmarkId == null) {
            Log.e(TAG, "Null bookmark found when showing the save flow, aborting.");
            return;
        }

        ShoppingService shoppingService = ShoppingServiceFactory.getForProfile(profile);
        UserEducationHelper userEducationHelper =
                new UserEducationHelper(activity, profile, new Handler(Looper.myLooper()));
        // Redirect the original profile when getting the identity manager, it's not done
        // automatically in native.
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile.getOriginalProfile());

        BookmarkSaveFlowCoordinator bookmarkSaveFlowCoordinator =
                new BookmarkSaveFlowCoordinator(
                        activity,
                        bottomSheetController,
                        shoppingService,
                        userEducationHelper,
                        profile,
                        identityManager,
                        bookmarkManagerOpener);
        bookmarkSaveFlowCoordinator.show(
                bookmarkId, fromExplicitTrackUi, wasBookmarkMoved, isNewBookmark);
    }

    // The legacy code path to add or edit bookmark without triggering the bookmark bottom sheet.
    // Used for feed and GTS.
    private static BookmarkId addBookmarkAndShowSnackbar(
            BookmarkModel bookmarkModel,
            Tab tab,
            SnackbarManager snackbarManager,
            Activity activity,
            boolean fromCustomTab,
            @BookmarkType int bookmarkType,
            BookmarkManagerOpener bookmarkManagerOpener) {
        BookmarkId parentId = null;
        if (bookmarkType == BookmarkType.READING_LIST) {
            parentId = bookmarkModel.getDefaultReadingListFolder();
        }
        BookmarkId bookmarkId =
                addBookmarkInternal(
                        activity,
                        tab.getProfile(),
                        bookmarkModel,
                        tab.getTitle(),
                        tab.getOriginalUrl(),
                        /* parent= */ parentId,
                        BookmarkType.NORMAL);

        Snackbar snackbar;
        if (bookmarkId == null) {
            snackbar =
                    Snackbar.make(
                                    activity.getString(R.string.bookmark_page_failed),
                                    new SnackbarController() {
                                        @Override
                                        public void onDismissNoAction(Object actionData) {}

                                        @Override
                                        public void onAction(Object actionData) {}
                                    },
                                    Snackbar.TYPE_NOTIFICATION,
                                    Snackbar.UMA_BOOKMARK_ADDED)
                            .setSingleLine(false);
            RecordUserAction.record("EnhancedBookmarks.AddingFailed");
        } else {
            String folderName =
                    bookmarkModel.getBookmarkTitle(
                            bookmarkModel.getBookmarkById(bookmarkId).getParentId());
            SnackbarController snackbarController =
                    createSnackbarControllerForEditButton(
                            activity, tab.getProfile(), bookmarkId, bookmarkManagerOpener);
            if (getLastUsedParent() == null) {
                if (fromCustomTab) {
                    String packageLabel = BuildInfo.getInstance().hostPackageLabel;
                    snackbar =
                            Snackbar.make(
                                    activity.getString(R.string.bookmark_page_saved, packageLabel),
                                    snackbarController,
                                    Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_BOOKMARK_ADDED);
                } else {
                    snackbar =
                            Snackbar.make(
                                    activity.getString(R.string.bookmark_page_saved_default),
                                    snackbarController,
                                    Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_BOOKMARK_ADDED);
                }
            } else {
                snackbar =
                        Snackbar.make(
                                activity.getString(R.string.bookmark_page_saved_folder, folderName),
                                snackbarController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_BOOKMARK_ADDED);
            }
            snackbar.setSingleLine(false)
                    .setAction(activity.getString(R.string.bookmark_item_edit), null);
        }
        snackbarManager.showSnackbar(snackbar);
        return bookmarkId;
    }

    /**
     * Add an article to the reading list. If the article was already loaded, the entry will be
     * overwritten. After successful addition, a snackbar will be shown notifying the user about the
     * result of the operation.
     *
     * @param activity The associated activity which is adding this reading list item.
     * @param bookmarkModel The bookmark model that talks to the bookmark backend.
     * @param title The title of the reading list item being added.
     * @param url The associated URL.
     * @param snackbarManager The snackbar manager that will be used to show a snackbar.
     * @param profile The profile currently used.
     * @param bottomSheetController The {@link BottomSheetController} which is used to show the
     *     BookmarkSaveFlow.
     * @return The bookmark ID created after saving the article to the reading list.
     * @deprecated Used only by feed, new users should rely on addOrEditBookmark (or the tab
     *     bookmarker).
     */
    @Deprecated
    public static BookmarkId addToReadingList(
            @NonNull Activity activity,
            @NonNull BookmarkModel bookmarkModel,
            @NonNull String title,
            @NonNull GURL url,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Profile profile,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull BookmarkManagerOpener bookmarkManagerOpener) {
        assert bookmarkModel.isBookmarkModelLoaded();
        BookmarkId bookmarkId =
                addBookmarkInternal(
                        activity,
                        profile,
                        bookmarkModel,
                        title,
                        url,
                        bookmarkModel.getDefaultReadingListFolder(),
                        BookmarkType.READING_LIST);
        if (bookmarkId == null) {
            return null;
        }

        // Reading list is aligned with the bookmark save flow used by all other bookmark saves.
        // This is bundled with account bookmarks to modernize the infra.
        if (bookmarkModel.areAccountBookmarkFoldersActive()) {
            showSaveFlow(
                    activity,
                    bottomSheetController,
                    profile,
                    bookmarkId,
                    /* fromExplicitTrackUi= */ false,
                    /* wasBookmarkMoved= */ false,
                    /* isNewBookmark= */ true,
                    bookmarkManagerOpener);
        } else {
            Snackbar snackbar =
                    Snackbar.make(
                            activity.getString(R.string.reading_list_saved),
                            new SnackbarController() {},
                            Snackbar.TYPE_ACTION,
                            Snackbar.UMA_READING_LIST_BOOKMARK_ADDED);
            snackbarManager.showSnackbar(snackbar);
        }

        TrackerFactory.getTrackerForProfile(profile)
                .notifyEvent(EventConstants.READ_LATER_ARTICLE_SAVED);
        return bookmarkId;
    }

    /**
     * Add all selected tabs from TabListEditor as bookmarks. This logic depends on the snackbar
     * workflow above. Currently there is no support for adding the selected tabs or newly created
     * folder directly to the reading list.
     *
     * @param activity The current activity.
     * @param bookmarkModel The bookmark model.
     * @param tabList The list of all currently selected tabs from the TabListEditor menu.
     * @param snackbarManager The SnackbarManager used to show the snackbar.
     */
    public static void addBookmarksOnMultiSelect(
            Activity activity,
            @NonNull BookmarkModel bookmarkModel,
            @NonNull List<Tab> tabList,
            @NonNull SnackbarManager snackbarManager,
            @NonNull BookmarkManagerOpener bookmarkManagerOpener) {
        // TODO(crbug.com/40879467): Refactor the bookmark folder select activity to allow for the
        // view to display in a dialog implementation approach.
        assert bookmarkModel != null;

        // For a single selected bookmark, default to the single tab-to-bookmark approach.
        if (tabList.size() == 1) {
            addBookmarkAndShowSnackbar(
                    bookmarkModel,
                    tabList.get(0),
                    snackbarManager,
                    activity,
                    false,
                    BookmarkType.NORMAL,
                    bookmarkManagerOpener);
            return;
        }

        // Current date time format with an example would be: Nov 17, 2022 4:34:20 PM PST
        DateFormat dateFormat =
                DateFormat.getDateTimeInstance(
                        DateFormat.MEDIUM, DateFormat.LONG, getLocale(activity));
        String fileName =
                activity.getString(
                        R.string.tab_selection_editor_add_bookmarks_folder_name,
                        dateFormat.format(new Date(System.currentTimeMillis())));
        BookmarkId newFolder =
                bookmarkModel.addFolder(bookmarkModel.getDefaultBookmarkFolder(), 0, fileName);
        int tabsBookmarkedCount = 0;

        Profile profile = null;
        for (Tab tab : tabList) {
            if (profile == null) {
                profile = tab.getProfile();
            } else {
                assert profile == tab.getProfile();
            }

            BookmarkId tabToBookmark =
                    addBookmarkInternal(
                            activity,
                            profile,
                            bookmarkModel,
                            tab.getTitle(),
                            tab.getOriginalUrl(),
                            newFolder,
                            BookmarkType.NORMAL);

            if (bookmarkModel.doesBookmarkExist(tabToBookmark)) {
                tabsBookmarkedCount++;
            }
        }
        RecordHistogram.recordCount100Histogram(
                "Android.TabMultiSelectV2.BookmarkTabsCount", tabsBookmarkedCount);

        SnackbarController snackbarController =
                createSnackbarControllerForBookmarkFolderEditButton(
                        activity, profile, newFolder, bookmarkManagerOpener);
        Snackbar snackbar =
                Snackbar.make(
                        activity.getString(R.string.bookmark_page_saved_default),
                        snackbarController,
                        Snackbar.TYPE_ACTION,
                        Snackbar.UMA_BOOKMARK_ADDED);
        snackbar.setSingleLine(false)
                .setAction(activity.getString(R.string.bookmark_item_edit), null);
        snackbarManager.showSnackbar(snackbar);
    }

    /**
     * Adds a bookmark with the given {@link Tab} without showing save flow.
     *
     * @param context The current Android {@link Context}.
     * @param tab The tab to add or edit a bookmark.
     * @param bookmarkModel The current {@link BookmarkModel} which talks to native.
     */
    public static BookmarkId addBookmarkWithoutShowingSaveFlow(
            Context context, Tab tab, BookmarkModel bookmarkModel) {
        BookmarkId parent =
                bookmarkModel.areAccountBookmarkFoldersActive()
                        ? bookmarkModel.getAccountMobileFolderId()
                        : bookmarkModel.getMobileFolderId();
        return addBookmarkInternal(
                context,
                tab.getProfile(),
                bookmarkModel,
                tab.getTitle(),
                tab.getOriginalUrl(),
                parent,
                BookmarkType.NORMAL);
    }

    /**
     * Adds a bookmark with the given {@link Tab}. This will reset last used parent if it fails to
     * add a bookmark.
     *
     * @param context The current Android {@link Context}.
     * @param profile The profile being used when adding the bookmark.
     * @param bookmarkModel The current {@link BookmarkModel} which talks to native.
     * @param title The title of the new bookmark.
     * @param url The {@link GURL} of the new bookmark.
     * @param bookmarkType The {@link BookmarkType} of the bookmark.
     * @param parent The {@link BookmarkId} which is the parent of the bookmark. If this is null,
     *     then the default parent is used.
     */
    static BookmarkId addBookmarkInternal(
            Context context,
            Profile profile,
            BookmarkModel bookmarkModel,
            String title,
            GURL url,
            @Nullable BookmarkId parent,
            @BookmarkType int bookmarkType) {
        parent = parent == null ? getLastUsedParent() : parent;
        BookmarkItem parentItem = null;
        if (parent != null) {
            parentItem = bookmarkModel.getBookmarkById(parent);
        }

        if (parent == null
                || parentItem == null
                || parentItem.isManaged()
                || !parentItem.isFolder()) {
            parent =
                    bookmarkType == BookmarkType.READING_LIST
                            ? bookmarkModel.getDefaultReadingListFolder()
                            : bookmarkModel.getDefaultBookmarkFolder();
        }

        // Reading list items will be added when either one of the 2 conditions is met:
        // 1. The bookmark type explicitly specifies READING_LIST.
        // 2. The last used parent implicitly specifies READING_LIST.
        final BookmarkId bookmarkId;
        if (bookmarkType == BookmarkType.READING_LIST
                || parent.getType() == BookmarkType.READING_LIST) {
            bookmarkId = bookmarkModel.addToReadingList(parent, title, url);
        } else {
            // Use "New tab" as title for both incognito and regular NTP.
            if (url.getSpec().equals(UrlConstants.NTP_URL)) {
                title = context.getString(R.string.new_tab_title);
            }

            bookmarkId =
                    bookmarkModel.addBookmark(
                            parent, bookmarkModel.getChildCount(parent), title, url);
        }

        if (bookmarkId != null) {
            BookmarkMetrics.recordBookmarkAdded(profile, bookmarkId);
            setLastUsedParent(parent);
        }
        return bookmarkId;
    }

    /**
     * Creates a snackbar controller for a case where "Edit" button is shown to edit the newly
     * created bookmark.
     */
    private static SnackbarController createSnackbarControllerForEditButton(
            final Activity activity,
            Profile profile,
            final BookmarkId bookmarkId,
            BookmarkManagerOpener bookmarkManagerOpener) {
        return new SnackbarController() {
            @Override
            public void onDismissNoAction(Object actionData) {
                RecordUserAction.record("EnhancedBookmarks.EditAfterCreateButtonNotClicked");
            }

            @Override
            public void onAction(Object actionData) {
                RecordUserAction.record("EnhancedBookmarks.EditAfterCreateButtonClicked");
                bookmarkManagerOpener.startEditActivity(activity, profile, bookmarkId);
            }
        };
    }

    /**
     * Creates a snackbar controller for a case where "Edit" button is shown to edit a newly created
     * bookmarks folder with bulk added bookmarks
     */
    private static SnackbarController createSnackbarControllerForBookmarkFolderEditButton(
            Activity activity,
            Profile profile,
            BookmarkId folder,
            BookmarkManagerOpener bookmarkManagerOpener) {
        return new SnackbarController() {
            @Override
            public void onDismissNoAction(Object actionData) {
                RecordUserAction.record("TabMultiSelectV2.BookmarkTabsSnackbarEditNotClicked");
            }

            @Override
            public void onAction(Object actionData) {
                RecordUserAction.record("TabMultiSelectV2.BookmarkTabsSnackbarEditClicked");
                bookmarkManagerOpener.startEditActivity(activity, profile, folder);
            }
        };
    }

    /**
     * Saves the last used url to preference. The saved url will be later queried by {@link
     * #getLastUsedUrl()}.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setLastUsedUrl(String url) {
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL, url);
    }

    /** Save the last used {@link BookmarkId} as a folder to put new bookmarks to. */
    public static void setLastUsedParent(BookmarkId bookmarkId) {
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT, bookmarkId.toString());
    }

    /**
     * @return The parent {@link BookmarkId} that the user used the last time or null if the user
     *     has never selected a parent folder to use.
     */
    public static @Nullable BookmarkId getLastUsedParent() {
        SharedPreferencesManager preferences = ChromeSharedPreferences.getInstance();
        if (!preferences.contains(ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT)) return null;

        return BookmarkId.getBookmarkIdFromString(
                preferences.readString(ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT, null));
    }

    /** Given the {@link BookmarkId}s, return a list of those ids serialized to string. */
    public static ArrayList<String> bookmarkIdsToStringList(BookmarkId... bookmarkIds) {
        ArrayList<String> bookmarkStrings = new ArrayList<>(bookmarkIds.length);
        for (BookmarkId id : bookmarkIds) {
            bookmarkStrings.add(id.toString());
        }

        return bookmarkStrings;
    }

    /**
     * Given the {@link BookmarkId}s serialized {@link String}s, return a list of the {@link
     * BookmarkId}s.
     */
    public static List<BookmarkId> stringListToBookmarkIds(
            BookmarkModel bookmarkModel, List<String> bookmarkIdStrings) {
        List<BookmarkId> bookmarkIds = new ArrayList<>(bookmarkIdStrings.size());
        for (String string : bookmarkIdStrings) {
            BookmarkId bookmarkId = BookmarkId.getBookmarkIdFromString(string);
            if (bookmarkModel.doesBookmarkExist(bookmarkId)) {
                bookmarkIds.add(bookmarkId);
            }
        }
        return bookmarkIds;
    }

    /**
     * @param context {@link Context} used to retrieve the drawable.
     * @param bookmarkId The bookmark id of the folder.
     * @param bookmarkModel The bookmark model.
     * @return A {@link Drawable} to use for displaying bookmark folders.
     */
    public static Drawable getFolderIcon(
            Context context,
            BookmarkId bookmarkId,
            BookmarkModel bookmarkModel,
            @BookmarkRowDisplayPref int displayPref) {
        ColorStateList tint = getFolderIconTint(context, bookmarkId.getType());
        if (bookmarkId.getType() == BookmarkType.READING_LIST) {
            return UiUtils.getTintedDrawable(context, R.drawable.ic_reading_list_folder_24dp, tint);
        } else if (bookmarkId.getType() == BookmarkType.NORMAL
                && Objects.equals(bookmarkId, bookmarkModel.getDesktopFolderId())) {
            return UiUtils.getTintedDrawable(context, R.drawable.ic_toolbar_24dp, tint);
        }

        return UiUtils.getTintedDrawable(
                context,
                displayPref == BookmarkRowDisplayPref.VISUAL
                        ? R.drawable.ic_folder_outline_24dp
                        : R.drawable.ic_folder_blue_24dp,
                tint);
    }

    /**
     * @param context {@link Context} used to retrieve the drawable.
     * @param type The bookmark type of the folder.
     * @return The tint used on the bookmark folder icon.
     */
    public static ColorStateList getFolderIconTint(Context context, @BookmarkType int type) {
        if (type == BookmarkType.READING_LIST) {
            return ColorStateList.valueOf(SemanticColorUtils.getDefaultIconColorAccent1(context));
        }

        return ColorStateList.valueOf(context.getColor(R.color.default_icon_color_tint_list));
    }

    /**
     * Expires the stored last used url if Chrome has been in the background long enough to mark it
     * as a new session. We're using the "Start Surface" concept of session here which is if the app
     * has been in the background for X amount of time. Called from #onStartWithNative, after which
     * the time stored in {@link ChromeInactivityTracker} is expired.
     *
     * @param timeSinceLastBackgroundedMs The time since Chrome has sent into the background.
     */
    public static void maybeExpireLastBookmarkLocationForReadLater(
            long timeSinceLastBackgroundedMs) {
        if (timeSinceLastBackgroundedMs > READING_LIST_SESSION_LENGTH_MS) {
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL);
        }
    }

    /** Returns whether this bookmark can be moved */
    public static boolean isMovable(BookmarkModel bookmarkModel, BookmarkItem item) {
        if (Objects.equals(item.getParentId(), bookmarkModel.getPartnerFolderId())) return false;
        return item.isEditable();
    }

    /**
     * Gets the display count for folders.
     *
     * @param id The bookmark to get the description for, must be a folder.
     * @param bookmarkModel The bookmark model to get info on the bookmark.
     */
    public static int getChildCountForDisplay(BookmarkId id, BookmarkModel bookmarkModel) {
        if (id.getType() == BookmarkType.READING_LIST) {
            return bookmarkModel.getUnreadCount(id);
        } else {
            return bookmarkModel.getTotalBookmarkCount(id);
        }
    }

    /**
     * Returns the description to use for the folder in bookmarks manager.
     *
     * @param id The bookmark to get the description for, must be a folder.
     * @param bookmarkModel The bookmark model to get info on the bookmark.
     * @param resources Android resources object to get strings.
     */
    public static String getFolderDescriptionText(
            BookmarkId id, BookmarkModel bookmarkModel, Resources resources) {
        int count = getChildCountForDisplay(id, bookmarkModel);
        if (id.getType() == BookmarkType.READING_LIST) {
            return (count > 0)
                    ? resources.getQuantityString(
                            R.plurals.reading_list_unread_page_count, count, count)
                    : resources.getString(R.string.reading_list_no_unread_pages);
        } else {
            return (count > 0)
                    ? resources.getQuantityString(R.plurals.bookmarks_count, count, count)
                    : resources.getString(R.string.no_bookmarks);
        }
    }

    /** Returns the RoundedIconGenerator with the appropriate size. */
    public static RoundedIconGenerator getRoundedIconGenerator(
            Context context, @BookmarkRowDisplayPref int displayPref) {
        Resources res = context.getResources();
        int iconSize = getFaviconDisplaySize(res);

        return displayPref == BookmarkRowDisplayPref.VISUAL
                ? new RoundedIconGenerator(
                        iconSize,
                        iconSize,
                        iconSize / 2,
                        context.getColor(R.color.default_favicon_background_color),
                        getDisplayTextSize(res))
                : FaviconUtils.createCircularIconGenerator(context);
    }

    /** Returns the size to use when fetching favicons. */
    public static int getFaviconFetchSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.tile_view_icon_min_size);
    }

    /** Returns the size to use when displaying an image. */
    public static int getImageIconSize(
            Resources resources, @BookmarkRowDisplayPref int displayPref) {
        return displayPref == BookmarkRowDisplayPref.VISUAL
                ? resources.getDimensionPixelSize(R.dimen.improved_bookmark_start_image_size_visual)
                : resources.getDimensionPixelSize(
                        R.dimen.improved_bookmark_start_image_size_compact);
    }

    /** Returns the size to use when displaying the favicon. */
    public static int getFaviconDisplaySize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.tile_view_icon_size_modern);
    }

    /**
     * Returns whether the given folder can have a folder added to it. Uses the base implementation
     * of {@link #canAddBookmarkToParent} with the additional constraint that a folder can't be
     * added to the reading list.
     */
    public static boolean canAddFolderToParent(BookmarkModel bookmarkModel, BookmarkId parentId) {
        if (!canAddBookmarkToParent(bookmarkModel, parentId)) {
            return false;
        }

        if (bookmarkModel.isReadingListFolder(parentId)) {
            return false;
        }

        return true;
    }

    /** Returns whether the given folder can have a bookmark added to it. */
    public static boolean canAddBookmarkToParent(BookmarkModel bookmarkModel, BookmarkId parentId) {
        BookmarkItem parentItem = bookmarkModel.getBookmarkById(parentId);
        if (parentItem == null) return false;
        if (parentItem.isManaged()) return false;
        if (Objects.equals(parentId, bookmarkModel.getPartnerFolderId())) return false;
        if (Objects.equals(parentId, bookmarkModel.getRootFolderId())) return false;

        return true;
    }

    /** Returns whether the given id is a special folder. */
    public static boolean isSpecialFolder(BookmarkModel bookmarkModel, BookmarkItem item) {
        return item != null && Objects.equals(item.getParentId(), bookmarkModel.getRootFolderId());
    }

    /** Return the background color for the given {@link BookmarkType}. */
    public static @ColorInt int getIconBackground(
            Context context, BookmarkModel bookmarkModel, BookmarkItem item) {
        if (isSpecialFolder(bookmarkModel, item)) {
            return SemanticColorUtils.getColorPrimaryContainer(context);
        } else {
            return ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_1);
        }
    }

    /** Return the icon tint for the given {@link BookmarkType}. */
    public static ColorStateList getIconTint(
            Context context, BookmarkModel bookmarkModel, BookmarkItem item) {
        if (isSpecialFolder(bookmarkModel, item)) {
            return ColorStateList.valueOf(
                    SemanticColorUtils.getDefaultIconColorOnAccent1Container(context));
        } else {
            return AppCompatResources.getColorStateList(
                    context, R.color.default_icon_color_secondary_tint_list);
        }
    }

    /** Returns whether the URL can be added as reading list article. */
    public static boolean isReadingListSupported(GURL url) {
        if (sReadingListSupportedForTesting != null) return sReadingListSupportedForTesting;
        if (url == null || url.isEmpty() || !url.isValid()) return false;

        // This should match ReadingListModel::IsUrlSupported(), having a separate function since
        // the UI may not load native library.
        return UrlUtilities.isHttpOrHttps(url);
    }

    private static int getDisplayTextSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.improved_bookmark_favicon_text_size);
    }

    private static Locale getLocale(Activity activity) {
        LocaleList locales = activity.getResources().getConfiguration().getLocales();
        if (locales.size() > 0) {
            return locales.get(0);
        }
        @SuppressWarnings("deprecation")
        Locale locale = activity.getResources().getConfiguration().locale;
        return locale;
    }
}
