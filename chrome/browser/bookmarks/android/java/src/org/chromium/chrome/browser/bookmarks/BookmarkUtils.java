// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.os.LocaleList;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApkInfo;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.signin.identitymanager.IdentityManager;
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
@NullMarked
public class BookmarkUtils {
    private static final String TAG = "BookmarkUtils";
    private static final int READING_LIST_SESSION_LENGTH_MS = (int) TimeUnit.HOURS.toMillis(1);

    private static @Nullable Boolean sReadingListSupportedForTesting;

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
     * @param bookmarkManagerOpener Manages opening bookmarks.
     * @param priceDropNotificationManager Manages price drop notifications.
     */
    public static void addOrEditBookmark(
            @Nullable BookmarkItem existingBookmarkItem,
            BookmarkModel bookmarkModel,
            Tab tab,
            BottomSheetController bottomSheetController,
            Activity activity,
            @BookmarkType int bookmarkType,
            Callback<@Nullable BookmarkId> callback,
            boolean fromExplicitTrackUi,
            BookmarkManagerOpener bookmarkManagerOpener,
            PriceDropNotificationManager priceDropNotificationManager,
            boolean isBookmarkBarVisible) {
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
                        bookmarkType,
                        isBookmarkBarVisible);
        showSaveFlow(
                activity,
                bottomSheetController,
                tab.getProfile(),
                newBookmarkId,
                fromExplicitTrackUi,
                /* wasBookmarkMoved= */ false,
                /* isNewBookmark= */ true,
                bookmarkManagerOpener,
                priceDropNotificationManager);
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
     * @param bookmarkManagerOpener Manages opening bookmarks.
     * @param priceDropNotificationManager Manages price drop notifications.
     */
    static void showSaveFlow(
            Activity activity,
            BottomSheetController bottomSheetController,
            Profile profile,
            @Nullable BookmarkId bookmarkId,
            boolean fromExplicitTrackUi,
            boolean wasBookmarkMoved,
            boolean isNewBookmark,
            BookmarkManagerOpener bookmarkManagerOpener,
            PriceDropNotificationManager priceDropNotificationManager) {
        if (bookmarkId == null) {
            Log.e(TAG, "Null bookmark found when showing the save flow, aborting.");
            return;
        }

        ShoppingService shoppingService = ShoppingServiceFactory.getForProfile(profile);
        UserEducationHelper userEducationHelper =
                new UserEducationHelper(
                        activity, profile, new Handler(assumeNonNull(Looper.myLooper())));
        // Redirect the original profile when getting the identity manager, it's not done
        // automatically in native.
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile.getOriginalProfile());
        assumeNonNull(identityManager);

        BookmarkSaveFlowCoordinator bookmarkSaveFlowCoordinator =
                new BookmarkSaveFlowCoordinator(
                        activity,
                        bottomSheetController,
                        shoppingService,
                        userEducationHelper,
                        profile,
                        identityManager,
                        bookmarkManagerOpener,
                        priceDropNotificationManager);
        bookmarkSaveFlowCoordinator.show(
                bookmarkId, fromExplicitTrackUi, wasBookmarkMoved, isNewBookmark);
    }

    // The legacy code path to add or edit bookmark without triggering the bookmark bottom sheet.
    // Used for feed and GTS.
    private static @Nullable BookmarkId addBookmarkAndShowSnackbar(
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
                                        public void onDismissNoAction(
                                                @Nullable Object actionData) {}

                                        @Override
                                        public void onAction(@Nullable Object actionData) {}
                                    },
                                    Snackbar.TYPE_NOTIFICATION,
                                    Snackbar.UMA_BOOKMARK_ADDED)
                            .setDefaultLines(false);
            RecordUserAction.record("EnhancedBookmarks.AddingFailed");
        } else {
            String folderName =
                    bookmarkModel.getBookmarkTitle(
                            assumeNonNull(bookmarkModel.getBookmarkById(bookmarkId)).getParentId());
            SnackbarController snackbarController =
                    createSnackbarControllerForEditButton(
                            activity, tab.getProfile(), bookmarkId, bookmarkManagerOpener);
            if (getLastUsedParent() == null) {
                if (fromCustomTab) {
                    String packageLabel = ApkInfo.getHostPackageLabel();
                    snackbar =
                            Snackbar.make(
                                    activity.getString(R.string.bookmark_page_saved, packageLabel),
                                    snackbarController,
                                    Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_BOOKMARK_ADDED);
                } else if (folderName != null && !folderName.isEmpty()) {
                    // We may have a folderName even without a last used profile, since saving to
                    // the default location doesn't update the last used parent automatically.
                    snackbar =
                            Snackbar.make(
                                    activity.getString(
                                            R.string.bookmark_page_saved_folder, folderName),
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
            snackbar.setDefaultLines(false)
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
     * @param bookmarkManagerOpener Manages opening bookmarks.
     * @param priceDropNotificationManager Manages price drop notifications.
     * @return The bookmark ID created after saving the article to the reading list.
     * @deprecated Used only by feed, new users should rely on addOrEditBookmark (or the tab
     *     bookmarker).
     */
    @Deprecated
    public static @Nullable BookmarkId addToReadingList(
            Activity activity,
            BookmarkModel bookmarkModel,
            String title,
            GURL url,
            SnackbarManager snackbarManager,
            Profile profile,
            BottomSheetController bottomSheetController,
            BookmarkManagerOpener bookmarkManagerOpener,
            PriceDropNotificationManager priceDropNotificationManager) {
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
                    bookmarkManagerOpener,
                    priceDropNotificationManager);
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
            BookmarkModel bookmarkModel,
            List<Tab> tabList,
            SnackbarManager snackbarManager,
            BookmarkManagerOpener bookmarkManagerOpener) {
        // TODO(crbug.com/40879467): Refactor the bookmark folder select activity to allow for the
        // view to display in a dialog implementation approach.
        assert bookmarkModel != null && !tabList.isEmpty();

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

        assumeNonNull(newFolder);

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
                        activity, assumeNonNull(profile), newFolder, bookmarkManagerOpener);
        Snackbar snackbar =
                Snackbar.make(
                        activity.getString(R.string.bookmark_page_saved_default),
                        snackbarController,
                        Snackbar.TYPE_ACTION,
                        Snackbar.UMA_BOOKMARK_ADDED);
        snackbar.setDefaultLines(false)
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
    public static @Nullable BookmarkId addBookmarkWithoutShowingSaveFlow(
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

    static @Nullable BookmarkId addBookmarkInternal(
            Context context,
            Profile profile,
            BookmarkModel bookmarkModel,
            String title,
            GURL url,
            @Nullable BookmarkId parent,
            @BookmarkType int bookmarkType) {
        return addBookmarkInternal(
                context, profile, bookmarkModel, title, url, parent, bookmarkType, false);
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
     * @param isBookmarkBarVisible True when the user is currently showing the bookmark bar.
     */
    static @Nullable BookmarkId addBookmarkInternal(
            Context context,
            Profile profile,
            BookmarkModel bookmarkModel,
            String title,
            GURL url,
            @Nullable BookmarkId parent,
            @BookmarkType int bookmarkType,
            boolean isBookmarkBarVisible) {
        parent = parent == null ? getLastUsedParent() : parent;

        // When the user did not have a last used parent, then we will usually save into a default
        // folder, which for normal bookmarks is the default bookmark folder ("Mobile Bookmarks" on
        // mobile devices). In the special case of the Bookmark Bar being visible, we will instead
        // make this the default by setting the parent here. However, we don't want this choice to
        // update the last used, because if the user hides the bookmark bar, we do not want to
        // continue saving to it unless the user explicitly chose to from the Edit dialog.
        boolean shouldSetAsLastUsed = true;
        if (parent == null && bookmarkType == BookmarkType.NORMAL) {
            if (isBookmarkBarVisible) {
                parent =
                        bookmarkModel.areAccountBookmarkFoldersActive()
                                ? bookmarkModel.getAccountDesktopFolderId()
                                : bookmarkModel.getDesktopFolderId();
                shouldSetAsLastUsed = false;
            }
        }

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

            // When we had to fall back on a default item, we do not also want to set this as the
            // last used location. Without setting this as the last used location, it will still be
            // returned in future bookmark actions (same behavior as setting it). However, if we do
            // set this as last used, then if in the future the user opens the bookmark bar, we
            // would not save by default to the bookmark bar like desired. If the user explicitly
            // picks the default location from then Edit dialog, then it it saved.
            if (bookmarkType == BookmarkType.NORMAL) {
                shouldSetAsLastUsed = false;
            }
        }
        assumeNonNull(parent);

        // Reading list items will be added when either one of the 2 conditions is met:
        // 1. The bookmark type explicitly specifies READING_LIST.
        // 2. The last used parent implicitly specifies READING_LIST.
        final BookmarkId bookmarkId;
        if (bookmarkType == BookmarkType.READING_LIST
                || parent.getType() == BookmarkType.READING_LIST) {
            bookmarkId = bookmarkModel.addToReadingList(parent, title, url);
        } else {
            UrlConstantResolver urlConstantResolver =
                    UrlConstantResolverFactory.getForProfile(profile);
            // Use "New tab" as title for both incognito and regular NTP.
            if (url.getSpec().equals(urlConstantResolver.getNtpUrl())) {
                title = context.getString(R.string.new_tab_title);
            }

            bookmarkId =
                    bookmarkModel.addBookmark(
                            parent, bookmarkModel.getChildCount(parent), title, url);
        }

        if (bookmarkId == null) {
            // Adding bookmark failed, so clear cache for parent bookmark folder
            RecordUserAction.record("BookmarkAdded.Failure");
            BookmarkBridge.clearLastUsedParent();
        } else {
            BookmarkMetrics.recordBookmarkAdded(profile, bookmarkId);
            if (shouldSetAsLastUsed) {
                setLastUsedParent(parent);
            }
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
            public void onDismissNoAction(@Nullable Object actionData) {
                RecordUserAction.record("EnhancedBookmarks.EditAfterCreateButtonNotClicked");
            }

            @Override
            public void onAction(@Nullable Object actionData) {
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
            public void onDismissNoAction(@Nullable Object actionData) {
                RecordUserAction.record("TabMultiSelectV2.BookmarkTabsSnackbarEditNotClicked");
            }

            @Override
            public void onAction(@Nullable Object actionData) {
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
     * Returns whether the given folder can have a folder added to it. Uses the base implementation
     * of {@link #canAddBookmarkToParent} with the additional constraint that a folder can't be
     * added to the reading list.
     */
    public static boolean canAddFolderToParent(
            BookmarkModel bookmarkModel, @Nullable BookmarkId parentId) {
        if (!canAddBookmarkToParent(bookmarkModel, parentId)) {
            return false;
        }

        if (bookmarkModel.isReadingListFolder(parentId)) {
            return false;
        }

        return true;
    }

    /** Returns whether the given folder can have a bookmark added to it. */
    public static boolean canAddBookmarkToParent(
            BookmarkModel bookmarkModel, @Nullable BookmarkId parentId) {
        BookmarkItem parentItem = bookmarkModel.getBookmarkById(parentId);
        if (parentItem == null) return false;
        if (parentItem.isManaged()) return false;
        if (Objects.equals(parentId, bookmarkModel.getPartnerFolderId())) return false;
        if (Objects.equals(parentId, bookmarkModel.getRootFolderId())) return false;

        return true;
    }

    /** Returns whether the URL can be added as reading list article. */
    public static boolean isReadingListSupported(GURL url) {
        if (sReadingListSupportedForTesting != null) return sReadingListSupportedForTesting;
        if (url == null || url.isEmpty() || !url.isValid()) return false;

        // This should match ReadingListModel::IsUrlSupported(), having a separate function since
        // the UI may not load native library.
        return UrlUtilities.isHttpOrHttps(url);
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
