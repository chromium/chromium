// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.expandedplayer;

import android.content.Context;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.readaloud.PlayerState;
import org.chromium.chrome.modules.readaloud.ExpandedPlayer;
import org.chromium.chrome.modules.readaloud.ExpandedPlayer.Observer;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

public class ExpandedPlayerCoordinator implements ExpandedPlayer {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final ObserverList<Observer> mObserverList;
    private ExpandedPlayerSheetContent mSheetContent;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor<PropertyModel, ExpandedPlayerSheetContent, PropertyKey>
            mModelChangeProcessor;
    private ExpandedPlayerMediator mMediator;

    public ExpandedPlayerCoordinator(Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mObserverList = new ObserverList<Observer>();
    }

    @Override
    public void addObserver(Observer observer) {
        mObserverList.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObserverList.removeObserver(observer);
    }

    @Override
    public void show(Playback playback) {
        assert playback != null;
        if (mSheetContent == null) {
            mSheetContent = new ExpandedPlayerSheetContent(mContext, mBottomSheetController);
            mModel = new PropertyModel.Builder(ExpandedPlayerProperties.ALL_KEYS)
                             .with(ExpandedPlayerProperties.STATE_KEY, PlayerState.GONE)
                             .build();
            mModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mModel, mSheetContent, ExpandedPlayerViewBinder::bind);
            mMediator = new ExpandedPlayerMediator(mBottomSheetController, mModel, new Observer() {
                @Override
                public void onCloseClicked() {
                    for (Observer observer : mObserverList) {
                        observer.onCloseClicked();
                    }
                }
            });
        }
        mMediator.show(playback);
    }

    @Override
    public void dismiss() {
        if (mMediator != null) {
            mMediator.dismiss();
        }
    }

    @Override
    public @PlayerState int getState() {
        if (mMediator == null) {
            return PlayerState.GONE;
        }
        return mMediator.getState();
    }

    BottomSheetContent getSheetContentForTesting() {
        return mSheetContent;
    }
}
