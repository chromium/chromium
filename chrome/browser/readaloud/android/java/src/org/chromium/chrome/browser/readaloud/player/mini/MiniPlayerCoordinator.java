// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator responsible for Read Aloud mini player lifecycle. */
public class MiniPlayerCoordinator {
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor<PropertyModel, MiniPlayerLayout, PropertyKey>
            mModelChangeProcessor;
    private final MiniPlayerMediator mMediator;
    private final MiniPlayerLayout mLayout;

    public MiniPlayerCoordinator(ViewStub viewStub, PropertyModel model) {
        this(viewStub, model, new MiniPlayerMediator(model));
    }

    @VisibleForTesting
    MiniPlayerCoordinator(ViewStub viewStub, PropertyModel model, MiniPlayerMediator mediator) {
        mModel = model;
        mLayout = (MiniPlayerLayout) viewStub.inflate();
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, mLayout, MiniPlayerViewBinder::bind);
        mMediator = mediator;
    }

    public void destroy() {
        if (mLayout != null) {
            mLayout.destroy();
        }
    }

    /**
     * Show the mini player if it isn't already showing.
     * @param animate True if the transition should be animated. If false, the mini player will
     *         instantly appear.
     */
    public void show(boolean animate) {
        mMediator.show(animate);
    }

    /**
     * Returns the mini player visibility state.
     */
    public @VisibilityState int getVisibility() {
        return mMediator.getVisibility();
    }

    /**
     * Dismiss the mini player.
     *
     * @param animate True if the transition should be animated. If false, the mini
     *                player will
     *                instantly disappear (though web contents resizing may lag
     *                behind).
     */
    public void dismiss(boolean animate) {
        mMediator.dismiss(animate);
    }
}
