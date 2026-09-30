// Copyright 2026 The Chromium Authors
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
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Controller for the Bookmark Bar In-Product Help. */
@NullMarked
public class BookmarkBarIphController extends BookmarkModelObserver implements Destroyable {
    private final Profile mProfile;
    private final BookmarkModel mBookmarkModel;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final UserEducationHelper mUserEducationHelper;
    private final NonNullObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;

    /**
     * @param activity The current activity.
     * @param profile The current user profile.
     * @param appMenuHandler The appMenuHandler, used to highlight the settings item.
     * @param toolbarMenuButton The toolbar menu button (3-dot) to which the IPH will be anchored.
     * @param bookmarkModel The bookmarkModel, which this class will observe for events.
     * @param xrSpaceModeObservableSupplier The supplier for the XR space mode state.
     */
    public BookmarkBarIphController(
            Activity activity,
            Profile profile,
            AppMenuHandler appMenuHandler,
            View toolbarMenuButton,
            BookmarkModel bookmarkModel,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        this(
                profile,
                appMenuHandler,
                toolbarMenuButton,
                bookmarkModel,
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper())),
                xrSpaceModeObservableSupplier);
    }

    @VisibleForTesting
    protected BookmarkBarIphController(
            Profile profile,
            AppMenuHandler appMenuHandler,
            View toolbarMenuButton,
            BookmarkModel bookmarkModel,
            UserEducationHelper userEducationHelper,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        mProfile = profile;
        mAppMenuHandler = appMenuHandler;
        mToolbarMenuButton = toolbarMenuButton;
        mBookmarkModel = bookmarkModel;
        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;
        // The BookmarkBarIphController object subscribes to mBookmarkModel.
        mBookmarkModel.addObserver(this);
        mUserEducationHelper = userEducationHelper;
        boolean mightTriggerIph =
                TrackerFactory.getTrackerForProfile(profile)
                        .wouldTriggerHelpUi(FeatureConstants.BOOKMARK_BAR_VISIBILITY_FEATURE);
        if (mightTriggerIph && passesPreChecks()) {
            // We have this so that the bookmark model is loaded when the app opens even when the
            // bookmark bar is invisible/disabled.
            mBookmarkModel.finishLoadingBookmarkModel(() -> {});
        }
    }

    @Override
    public void destroy() {
        mBookmarkModel.removeObserver(this);
    }

    /** Shows the In-Product Help text bubble. */
    @VisibleForTesting
    void showIph() {
        if (!passesPreChecks()) return;

        mToolbarMenuButton.post(
                () -> {
                    mUserEducationHelper.requestShowIph(
                            new IphCommandBuilder(
                                            mToolbarMenuButton.getContext().getResources(),
                                            FeatureConstants.BOOKMARK_BAR_VISIBILITY_FEATURE,
                                            R.string.bookmark_bar_iph_message,
                                            R.string.bookmark_bar_iph_message)
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

    // TODO(crbug.com/566865543): Implement v2 trigger and eligibility checks.
    @Override
    public void bookmarkModelChanged() {}

    /**
     * Runs all of the prerequisite checks for showing the IPH. These checks do not require the
     * bookmark model to be loaded.
     */
    private boolean passesPreChecks() {
        boolean isXrFullSpaceMode = mXrSpaceModeObservableSupplier.get();
        // Do not show the IPH in XR full space mode or if the activity state (e.g. window width)
        // is incompatible with the bookmark bar. Note: BookmarkBarUtils visibility methods also
        // check these, but they return false / ALWAYS_HIDE when in XR mode or incompatible, which
        // would otherwise pass the "bookmark bar is hidden" check below.
        if (isXrFullSpaceMode
                || !isActivityStateBookmarkBarCompatible(mToolbarMenuButton.getContext())) {
            return false;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_BAR_NTP)) {
            // If the bookmark bar is already visible (ALWAYS_SHOW or ONLY_SHOW_ON_NTP), or if the
            // user has explicitly set the device preference, do not show the IPH.
            if (BookmarkBarUtils.getBookmarkBarVisibilityState(
                            mToolbarMenuButton.getContext(), mProfile, isXrFullSpaceMode)
                    != BookmarkBarVisibilityState.ALWAYS_HIDE) {
                return false;
            }
            if (BookmarkBarUtils.hasUserSetDevicePrefBookmarkBarVisibilityState()) {
                return false;
            }
        } else {
            // If the bookmark bar is already visible, or if the user has explicitly set the device
            // preference, do not show the IPH.
            if (BookmarkBarUtils.isBookmarkBarVisible(
                    mToolbarMenuButton.getContext(), mProfile, isXrFullSpaceMode)) {
                return false;
            }
            if (BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar()) {
                return false;
            }
        }

        // All checks passed.
        return true;
    }
}
