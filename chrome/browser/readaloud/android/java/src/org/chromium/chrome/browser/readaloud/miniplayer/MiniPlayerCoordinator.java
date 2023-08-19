// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.miniplayer;

import android.view.View;
import android.view.ViewStub;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.readaloud.PlayerState;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator responsible for Read Aloud mini player lifecycle. */
public class MiniPlayerCoordinator {
    private final ViewStub mViewStub;
    private final ObserverList<Observer> mObserverList;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor<PropertyModel, LinearLayout, PropertyKey>
            mModelChangeProcessor;
    private MiniPlayerMediator mMediator;
    private LinearLayout mLayout;

    /** Interface for receiving updates about the mini player. */
    public interface Observer {
        /** Called when the user taps the mini player to make it expand. */
        void onExpandRequested();
        /** Called when the user taps the close button. */
        void onCloseClicked();
    }

    public MiniPlayerCoordinator(ViewStub viewStub) {
        assert viewStub != null;
        mViewStub = viewStub;
        mObserverList = new ObserverList<Observer>();
    }

    /**
     * Add an observer to receive mini player updates.
     * @param observer Observer to add.
     */
    public void addObserver(Observer observer) {
        mObserverList.addObserver(observer);
    }

    /**
     * Remove an observer that was previously added. No effect if the observer was never added.
     */
    public void removeObserver(Observer observer) {
        mObserverList.removeObserver(observer);
    }

    /**
     * Show the mini player if it isn't already showing.
     * @param animate True if the transition should be animated. If false, the mini player will
     *         instantly appear.
     * @param playback Playback object. Pass null if playback isn't available yet and a loading
     *         indicator should be shown.
     */
    public void show(boolean animate, @Nullable Playback playback) {
        if (mLayout == null) {
            mLayout = (LinearLayout) mViewStub.inflate();
            mModel = new PropertyModel.Builder(MiniPlayerProperties.ALL_KEYS)
                             .with(MiniPlayerProperties.PLAYER_STATE_KEY, PlayerState.GONE)
                             .with(MiniPlayerProperties.VIEW_VISIBILITY_KEY, View.GONE)
                             .with(MiniPlayerProperties.TITLE_KEY, "Title")
                             .with(MiniPlayerProperties.PUBLISHER_KEY, "Publisher")
                             .build();
            mModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mModel, mLayout, MiniPlayerViewBinder::bind);
            mMediator = new MiniPlayerMediator(mModel, new Observer() {
                @Override
                public void onExpandRequested() {
                    for (Observer observer : mObserverList) {
                        observer.onExpandRequested();
                    }
                }
                @Override
                public void onCloseClicked() {
                    for (Observer observer : mObserverList) {
                        observer.onCloseClicked();
                    }
                }
            });
        }
        mMediator.show(animate, playback);
    }

    /**
     * Returns the mini player visibility state.
     */
    public @PlayerState int getState() {
        if (mMediator == null) {
            return PlayerState.GONE;
        }
        return mMediator.getState();
    }

    /**
     * Dismiss the mini player.
     * @param animate True if the transition should be animated. If false, the mini player will
     *         instantly disappear (though web contents resizing may lag behind).
     */
    public void dismiss(boolean animate) {
        if (mMediator == null) {
            return;
        }
        mMediator.dismiss(animate);
    }
}
