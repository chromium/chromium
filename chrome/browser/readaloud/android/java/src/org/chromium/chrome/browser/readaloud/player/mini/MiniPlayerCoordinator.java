// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import android.view.ViewStub;

import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator responsible for Read Aloud mini player lifecycle. */
public class MiniPlayerCoordinator {
    private final ViewStub mViewStub;
    private final PropertyModel mModel;
    private PropertyModelChangeProcessor<PropertyModel, MiniPlayerLayout, PropertyKey>
            mModelChangeProcessor;
    private MiniPlayerMediator mMediator;
    private MiniPlayerLayout mLayout;

    public MiniPlayerCoordinator(ViewStub viewStub, PropertyModel model) {
        assert viewStub != null;
        mViewStub = viewStub;
        mModel = model;
    }

    /**
     * Show the mini player if it isn't already showing.
     * @param animate True if the transition should be animated. If false, the mini player will
     *         instantly appear.
     */
    public void show(boolean animate) {
        if (mLayout == null) {
            mLayout = (MiniPlayerLayout) mViewStub.inflate();
            mModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mModel, mLayout, MiniPlayerViewBinder::bind);
            mMediator = new MiniPlayerMediator(mModel);
        }
        mMediator.show(animate);
    }

    /**
     * Returns the mini player visibility state.
     */
    public @VisibilityState int getVisibility() {
        if (mMediator == null) {
            return VisibilityState.GONE;
        }
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
        if (mMediator == null) {
            return;
        }
        mMediator.dismiss(animate);
    }
}
