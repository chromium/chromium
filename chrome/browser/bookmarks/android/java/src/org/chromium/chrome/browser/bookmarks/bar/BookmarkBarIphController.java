// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils.isActivityStateBookmarkBarCompatible;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.feature_engagement.FeatureConstants;

import java.util.List;

/** Controller for the Bookmarks Bar In-Product Help. */
@NullMarked
public class BookmarkBarIphController extends BookmarkModelObserver implements Destroyable {
    private final Profile mProfile;
    private final BookmarkModel mBookmarkModel;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final UserEducationHelper mUserEducationHelper;

    /**
     * @param activity The current activity.
     * @param profile The current user profile.
     * @param appMenuHandler The appMenuHandler, used to highlight the settings item.
     * @param toolbarMenuButton The toolbar menu button (3-dot) to which the IPH will be anchored.
     * @param bookmarkModel The bookmarkModel, which this class will observe for events.
     */
    public BookmarkBarIphController(
            Activity activity,
            Profile profile,
            AppMenuHandler appMenuHandler,
            View toolbarMenuButton,
            BookmarkModel bookmarkModel) {
        this(
                profile,
                appMenuHandler,
                toolbarMenuButton,
                bookmarkModel,
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper())));
    }

    @VisibleForTesting
    protected BookmarkBarIphController(
            Profile profile,
            AppMenuHandler appMenuHandler,
            View toolbarMenuButton,
            BookmarkModel bookmarkModel,
            UserEducationHelper userEducationHelper) {
        mProfile = profile;
        mAppMenuHandler = appMenuHandler;
        mToolbarMenuButton = toolbarMenuButton;
        mBookmarkModel = bookmarkModel;
        // The BookmarkBarIphController object subscribes to mBookmarkModel.
        mBookmarkModel.addObserver(this);
        mUserEducationHelper = userEducationHelper;
        boolean mightTriggerIph =
                TrackerFactory.getTrackerForProfile(profile)
                        .wouldTriggerHelpUi(FeatureConstants.BOOKMARKS_BAR_FEATURE);
        if (mightTriggerIph && passesPreChecks()) {
            // We have this so that the bookmark model is loaded when the app opens even when the
            // bookmarks bar is invisible/disabled.
            mBookmarkModel.finishLoadingBookmarkModel(() -> {});
        }
    }

    @Override
    public void destroy() {
        mBookmarkModel.removeObserver(this);
    }

    /** This callback ensures we only access the bookmark model after it has been fully loaded. */
    @Override
    public void bookmarkModelLoaded() {
        // Trigger condition 1: Check for existing bookmarks only in the bookmarks bar (not mobile
        // bookmarks or reading list) now that the bookmark model is loaded.
        if (hasAtLeastOneBookmarkInBookmarksBar()) {
            showIPH();
        }
    }

    @Override
    public void bookmarkNodeAdded(BookmarkItem parent, int index, boolean addedByUser) {
        // Only trigger the IPH if the bookmark was added by the user on this device.
        // This ignores bookmarks that are added via sync.
        if (!addedByUser) return;

        BookmarkId addedId = mBookmarkModel.getChildIds(parent.getId()).get(index);
        BookmarkItem addedItem = mBookmarkModel.getBookmarkById(addedId);

        // The Iph is triggered only when an actual bookmark, and not an empty folder, is added.
        if (addedItem == null || addedItem.isFolder()) return;

        // Trigger condition 2: A new bookmark was added to the bookmarks bar, mobile bookmarks, or
        // reading list.
        showIPH();
    }

    public boolean hasAtLeastOneBookmark(@Nullable BookmarkId folderId) {
        if (folderId == null) return false;

        // Get all of the children in the bookmarks bar, which may include both bookmarks and
        // folders.
        List<BookmarkId> children = mBookmarkModel.getChildIds(folderId);
        for (BookmarkId childId : children) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(childId);
            if (item != null && !item.isFolder()) {
                return true;
            }
        }
        return false;
    }

    public boolean hasAtLeastOneBookmarkInBookmarksBar() {
        // Check the local/device bookmarks bar.
        if (hasAtLeastOneBookmark(mBookmarkModel.getDesktopFolderId())) {
            return true;
        }

        // Check the account/synced bookmarks bar.
        if (hasAtLeastOneBookmark(mBookmarkModel.getAccountDesktopFolderId())) {
            return true;
        }

        // No bookmarks were found.
        return false;
    }

    /** Shows the In-Product Help text bubble. */
    private void showIPH() {
        // Mainly for Trigger condition 2. prechecks for condition 1 is in the constructor.
        if (!passesPreChecks()) return;

        mToolbarMenuButton.post(
                () -> {
                    mUserEducationHelper.requestShowIph(
                            new IphCommandBuilder(
                                            mToolbarMenuButton.getContext().getResources(),
                                            FeatureConstants.BOOKMARKS_BAR_FEATURE,
                                            R.string.bookmarks_bar_iph_message,
                                            R.string.bookmarks_bar_iph_message)
                                    .setAnchorView(mToolbarMenuButton)
                                    .setOnShowCallback(
                                            () ->
                                                    // Highlight the app menu settings item.
                                                    mAppMenuHandler.setMenuHighlight(
                                                            R.id.preferences_id))
                                    .setOnDismissCallback(mAppMenuHandler::clearMenuHighlight)
                                    .build());
                });
    }

    @Override
    public void bookmarkModelChanged() {}

    /**
     * Runs all of the prerequisite checks for showing the IPH. These checks do not require the
     * bookmark model to be loaded.
     */
    private boolean passesPreChecks() {
        // If the bookmark bar is already visible, there's no reason to show the IPH.
        if (BookmarkBarUtils.isBookmarkBarVisible(mToolbarMenuButton.getContext(), mProfile))
            return false;

        if (BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar()) return false;

        // Checks the device compatibility and that the window is wide enough to show the bookmarks
        // bar.
        if (!isActivityStateBookmarkBarCompatible(mToolbarMenuButton.getContext())) return false;

        // All checks passed.
        return true;
    }
}
