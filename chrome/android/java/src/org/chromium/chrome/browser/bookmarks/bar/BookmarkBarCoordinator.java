// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.view.ViewStub;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the bookmark bar which provides users with bookmark access from top chrome. */
public class BookmarkBarCoordinator {

    private final BookmarkBarMediator mMediator;
    private final BookmarkBar mView;

    /**
     * Constructs the bookmark bar coordinator.
     *
     * @param activity the activity which is hosting the bookmark bar.
     * @param browserControlsManager the manager for browser control positioning/visibility.
     * @param heightChangeCallback a callback to notify of bookmark bar height change events.
     * @param profileSupplier the supplier for the currently active profile.
     * @param viewStub the stub used to inflate the bookmark bar.
     */
    public BookmarkBarCoordinator(
            @NonNull Activity activity,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull Callback<Integer> heightChangeCallback,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull ViewStub viewStub) {
        mView = (BookmarkBar) viewStub.inflate();

        // Bind view/model for 'All Bookmarks' button.
        final var allBookmarksButtonModel =
                new PropertyModel.Builder(BookmarkBarButtonProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                allBookmarksButtonModel,
                mView.findViewById(R.id.bookmark_bar_all_bookmarks_button),
                BookmarkBarButtonViewBinder::bind);

        // Bind view/model for bookmark bar and instantiate mediator.
        final var model = new PropertyModel.Builder(BookmarkBarProperties.ALL_KEYS).build();
        mMediator =
                new BookmarkBarMediator(
                        activity,
                        allBookmarksButtonModel,
                        browserControlsManager,
                        heightChangeCallback,
                        model,
                        profileSupplier);
        PropertyModelChangeProcessor.create(model, mView, BookmarkBarViewBinder::bind);
    }

    /** Destroys the bookmark bar coordinator. */
    public void destroy() {
        mMediator.destroy();
        mView.destroy();
    }

    /**
     * @return the supplier which provides the current height of the bookmark bar.
     */
    public ObservableSupplier<Integer> getHeightSupplier() {
        return mMediator.getHeightSupplier();
    }
}
