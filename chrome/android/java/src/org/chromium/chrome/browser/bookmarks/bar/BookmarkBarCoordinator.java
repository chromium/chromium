// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.view.ViewStub;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the bookmark bar which provides users with bookmark access from top chrome. */
public class BookmarkBarCoordinator {

    private final BookmarkBarMediator mMediator;
    private final BookmarkBar mView;

    /**
     * Constructs the bookmark bar coordinator.
     *
     * @param browserControlsManager the manager for browser control positioning/visibility.
     * @param heightChangeCallback a callback to notify of bookmark bar height change events.
     * @param viewStub the stub used to inflate the bookmark bar.
     */
    public BookmarkBarCoordinator(
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull Callback<Integer> heightChangeCallback,
            @NonNull ViewStub viewStub) {
        final var model = new PropertyModel.Builder(BookmarkBarProperties.ALL_KEYS).build();
        mMediator = new BookmarkBarMediator(browserControlsManager, heightChangeCallback, model);
        mView = (BookmarkBar) viewStub.inflate();
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
