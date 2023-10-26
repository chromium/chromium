// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.readaloud.player.R;
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

    /**
     * @param activity App activity containing a placeholder FrameLayout with ID
     *     R.id.readaloud_mini_player.
     * @param context View-inflation-capable Context for read_aloud_playback isolated split.
     * @param model Player UI property model.
     */
    public MiniPlayerCoordinator(Activity activity, Context context, PropertyModel model) {
        this(model, new MiniPlayerMediator(model), inflateLayout(activity, context));
    }

    private static MiniPlayerLayout inflateLayout(Activity activity, Context context) {
        ViewStub stub = activity.findViewById(R.id.readaloud_mini_player_stub);
        assert stub != null;
        stub.setLayoutResource(R.layout.readaloud_mini_player_layout);
        stub.setLayoutInflater(LayoutInflater.from(context));
        return (MiniPlayerLayout) stub.inflate();
    }

    @VisibleForTesting
    MiniPlayerCoordinator(
            PropertyModel model, MiniPlayerMediator mediator, MiniPlayerLayout layout) {
        mModel = model;
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, layout, MiniPlayerViewBinder::bind);
        mMediator = mediator;
        mLayout = layout;
        assert layout != null;
    }

    public void destroy() {
        mLayout.destroy();
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
