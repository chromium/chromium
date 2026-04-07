// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.function.Supplier;

/** Handles the "bookmark all tabs" action. */
@NullMarked
public class BookmarkAllTabsHandler implements MenuOrKeyboardActionHandler {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(BookmarkAllTabsResult)
    @IntDef({
        BookmarkAllTabsResult.SUCCESS,
        BookmarkAllTabsResult.MODEL_NULL,
        BookmarkAllTabsResult.TAB_LIST_EMPTY,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkAllTabsResult {
        int SUCCESS = 0;
        int MODEL_NULL = 1;
        int TAB_LIST_EMPTY = 2;

        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidBookmarkAllTabsResult)
    private final Supplier<@Nullable SnackbarManager> mSnackbarManagerSupplier;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private final WindowAndroid mWindowAndroid;

    /**
     * @param snackbarManagerSupplier Supplier of the snackbar manager.
     * @param tabModelSelectorSupplier Supplier of the tab model selector.
     * @param windowAndroid The window associated with the action.
     */
    public BookmarkAllTabsHandler(
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            Supplier<@Nullable SnackbarManager> snackbarManagerSupplier,
            WindowAndroid windowAndroid) {
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mWindowAndroid = windowAndroid;
    }

    /**
     * Attempts to bookmark all tabs in the given TabModel.
     *
     * @param tabModel The TabModel representing the tabs.
     * @param windowAndroid The window associated with the action.
     * @param snackbarManager Manager for displaying snackbars.
     */
    public static void bookmarkAllTabs(
            @Nullable TabModel tabModel,
            WindowAndroid windowAndroid,
            SnackbarManager snackbarManager) {
        RecordUserAction.record("Android.BookmarkAllTabs");
        if (tabModel == null) {
            recordResult(BookmarkAllTabsResult.MODEL_NULL);
            return;
        }
        List<Tab> tabs = TabModelUtils.convertTabListToListOfTabs(tabModel);
        if (tabs.isEmpty()) {
            recordResult(BookmarkAllTabsResult.TAB_LIST_EMPTY);
            return;
        }

        Profile profile = tabModel.getProfile();
        if (profile == null) return;
        BookmarkModel bookmarkModel = BookmarkModel.getForProfile(profile);
        bookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    Activity activity = windowAndroid.getActivity().get();
                    if (activity == null) return;

                    recordResult(BookmarkAllTabsResult.SUCCESS);
                    BookmarkUtils.addTabsToBookmarks(
                            activity,
                            bookmarkModel,
                            tabs,
                            snackbarManager,
                            new BookmarkManagerOpenerImpl());
                });
    }

    private static void recordResult(@BookmarkAllTabsResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BookmarkAllTabs.Result", result, BookmarkAllTabsResult.NUM_ENTRIES);
    }

    @Override
    public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.bookmark_all_tabs) {
            TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
            SnackbarManager snackbarManager = mSnackbarManagerSupplier.get();
            if (tabModelSelector != null && snackbarManager != null) {
                bookmarkAllTabs(
                        tabModelSelector.getCurrentModel(), mWindowAndroid, snackbarManager);
            }
            return true;
        }
        return false;
    }
}
