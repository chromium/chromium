// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Objects;

/** Mediator for the bookmark bar which provides users with bookmark access from top chrome. */
class BookmarkBarMediator implements BrowserControlsStateProvider.Observer {

    private final Activity mActivity;
    private final PropertyModel mAllBookmarksButtonModel;
    private final BrowserControlsManager mBrowserControlsManager;
    private final Callback<Integer> mHeightChangeCallback;
    private final ObservableSupplierImpl<Integer> mHeightSupplier;
    private final PropertyModel mModel;
    private final ObservableSupplier<Profile> mProfileSupplier;

    /**
     * Constructs the bookmark bar mediator.
     *
     * @param activity the activity which is hosting the bookmark bar.
     * @param allBookmarksButtonModel the model for the 'All Bookmarks' button.
     * @param browserControlsManager the manager for browser control positioning/visibility.
     * @param heightChangeCallback a callback to notify of bookmark bar height change events.
     * @param model the model used to read/write bookmark bar properties.
     * @param profileSupplier the supplier for the currently active profile.
     */
    public BookmarkBarMediator(
            @NonNull Activity activity,
            @NonNull PropertyModel allBookmarksButtonModel,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull Callback<Integer> heightChangeCallback,
            @NonNull PropertyModel model,
            @NonNull ObservableSupplier<Profile> profileSupplier) {
        mActivity = activity;

        mAllBookmarksButtonModel = allBookmarksButtonModel;
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.CLICK_CALLBACK, this::onAllBookmarksButtonClick);
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.ICON,
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_folder_outline_24dp));
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.TITLE,
                mActivity.getString(R.string.bookmark_bar_all_bookmarks_button_title));

        mBrowserControlsManager = browserControlsManager;
        mBrowserControlsManager.addObserver(this);

        // NOTE: Height will be updated when binding the `HEIGHT_CHANGE_CALLBACK` property.
        mHeightSupplier = new ObservableSupplierImpl<Integer>(0);
        mHeightChangeCallback = heightChangeCallback;
        mHeightSupplier.addObserver(mHeightChangeCallback);

        mModel = model;
        mModel.set(BookmarkBarProperties.HEIGHT_CHANGE_CALLBACK, mHeightSupplier::set);

        mProfileSupplier = profileSupplier;

        updateTopMargin();
        updateVisibility();
    }

    /** Destroys the bookmark bar mediator. */
    public void destroy() {
        mAllBookmarksButtonModel.set(BookmarkBarButtonProperties.CLICK_CALLBACK, null);
        mBrowserControlsManager.removeObserver(this);
        mHeightSupplier.removeObserver(mHeightChangeCallback);
        mModel.set(BookmarkBarProperties.HEIGHT_CHANGE_CALLBACK, null);
    }

    /**
     * @return the supplier which provides the current height of the bookmark bar.
     */
    public ObservableSupplier<Integer> getHeightSupplier() {
        return mHeightSupplier;
    }

    private void onAllBookmarksButtonClick() {
        final var profile = mProfileSupplier.get();
        if (profile == null) return;

        final var model = BookmarkModel.getForProfile(profile);
        model.finishLoadingBookmarkModel(
                () -> {
                    // Ensure the active profile hasn't changed while loading the model.
                    final var profileAfterLoading = mProfileSupplier.get();
                    if (!Objects.equals(profile, profileAfterLoading)) return;

                    // Ensure the active model hasn't changed while loading the model.
                    final var modelAfterLoading = BookmarkModel.getForProfile(profileAfterLoading);
                    if (!Objects.equals(model, modelAfterLoading)) return;

                    // Open the manager iff the active profile and model is unchanged to prevent
                    // accidentally opening the manager for the wrong profile/model.
                    BookmarkUtils.showBookmarkManager(
                            mActivity,
                            modelAfterLoading.getRootFolderId(),
                            profileAfterLoading.isOffTheRecord());
                });
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            boolean topControlsMinHeightChanged,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean bottomControlsMinHeightChanged,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        updateVisibility();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateTopMargin();
    }

    // TODO(crbug.com/339492600): Replace w/ positioning construct akin to `BottomControlsStacker`.
    private void updateTopMargin() {
        // NOTE: Top controls height is the sum of all top browser control heights which includes
        // that of the bookmark bar. Subtract the bookmark bar's height from the top controls height
        // when calculating top margin in order to bottom align the bookmark bar relative to other
        // top browser controls.
        mModel.set(
                BookmarkBarProperties.TOP_MARGIN,
                mBrowserControlsManager.getTopControlsHeight() - mHeightSupplier.get());
    }

    private void updateVisibility() {
        mModel.set(
                BookmarkBarProperties.VISIBILITY,
                mBrowserControlsManager.getTopControlOffset() == 0 ? View.VISIBLE : View.GONE);
    }
}
